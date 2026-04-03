# Injection Engine Fix: SIGSEGV Crash Resolution

## Problem

The original `InjectPtrace` implementation in `stealth_injector.cpp` was causing SIGSEGV crashes during library injection. The root cause was using the target thread's original stack, which led to:

1. **Stack corruption**: The target thread's stack could be in use or corrupted
2. **Insufficient stack space**: dlopen and its dependencies need significant stack space
3. **Race conditions**: The original thread's stack pointer could conflict with our injection
4. **Red zone violations**: x86-64 ABI reserves 128 bytes below RSP for scratch space

## Solution

The fix allocates a dedicated 8KB memory block that contains everything needed for injection:

### Memory Layout (8KB Total)

```
+------------------+ 0x0000
|   Shellcode      | 256 bytes - Executable code to call dlopen
+------------------+ 0x0100
|   Path String    | 256 bytes - Library path for dlopen
+------------------+ 0x0200
|                  |
|   Stack Space    | ~7.5KB - Dedicated stack for dlopen execution
|   (grows down)   |
|                  |
+------------------+ 0x2000 (RSP starts here)
```

### Key Changes

1. **Single Allocation**: Instead of separate allocations for path and shellcode, we allocate one 8KB block with `PROT_READ | PROT_WRITE | PROT_EXEC`

2. **Dedicated Stack**: RSP is set to the top of the allocated block (0x2000), providing ~7.5KB of clean stack space

3. **Proper ABI Compliance**: 
   - RSP is 16-byte aligned (8KB is divisible by 16)
   - A dummy return address (zeros) is pushed before execution
   - Caller-saved registers (rcx, r10, r11) are zeroed

4. **Path Address Passing**: The library path address is correctly embedded in the shellcode at offset +12

### Code Changes

```cpp
// Allocate 8KB block for everything
constexpr size_t kAllocSize = 8192;
uintptr_t blockBase = AllocateRemoteMemoryInProcess(pid, tid, origRegs, kAllocSize,
                                                    PROT_READ | PROT_WRITE | PROT_EXEC);

// Layout within the block
uintptr_t shellcodeMem = blockBase + 0;      // Shellcode at start
uintptr_t pathMem = blockBase + 256;         // Path at offset 256
uintptr_t stackTop = blockBase + kAllocSize; // RSP at top

// Set up registers with dedicated stack
struct user_regs_struct regs = origRegs;
regs.rsp = stackTop;  // Point to top of our dedicated stack
regs.rsp -= 8;        // Push dummy return address
regs.rip = shellcodeMem;
regs.rcx = regs.r10 = regs.r11 = 0;  // Zero caller-saved registers
```

## Verification

After applying this fix, the injector should:

1. Successfully allocate the 8KB block
2. Print the memory addresses for debugging:
   ```
   [+] Allocated 8KB block at 0x7f...
   [+] Path at 0x7f...: /path/to/library.so
   [+] Shellcode at 0x7f... (29 bytes)
   [+] Setting RSP to 0x7f... (dedicated stack), RIP to 0x7f...
   ```

3. Complete injection without SIGSEGV
4. Return a valid library handle from dlopen

## Build

```bash
cmake --build build --target bloodstrike_injector
```

## Testing

```bash
# Verbose mode to see RSP diagnostics
./build/bloodstrike-injector -v -p <process> -l ./libbloodstrike.so
```

## Technical Details

### Why 8KB?

- Provides sufficient stack space for dlopen and its dependencies
- Small enough to avoid memory pressure
- Aligns well with page boundaries (multiple of 4096)
- Divisible by 16 for stack alignment requirements

### Why PROT_EXEC on the whole block?

While only the shellcode portion needs execute permission, applying it to the whole block:
- Simplifies the allocation (single mmap call)
- The block is small (8KB) so the security impact is minimal
- Avoids potential issues with mprotect on sub-regions

### Stack Growth Direction

The stack grows downward from the top of the block:
- RSP starts at `blockBase + 8192` (top)
- Each push decreases RSP by 8 bytes
- This provides maximum usable stack space

### Return Address Handling (SIGSEGV Fix v2)

The initial 8KB stack fix still caused SIGSEGV because the `call rax` instruction
pushes a return address onto the stack, and when dlopen returned, it would jump
to whatever garbage value was on the stack (previously 0 from our dummy push).

**Final Solution:**
1. Calculate the address of the `int3` instruction within our shellcode
2. Write this `int3` address to the stack as the return address BEFORE execution
3. When dlopen returns, it pops this address and jumps to `int3`, triggering SIGTRAP

```cpp
// Calculate int3 address within shellcode
const size_t kInt3Offset = 27;  // int3 is at byte 27
uintptr_t int3Addr = shellcodeMem + kInt3Offset;

// Write return address to stack
uintptr_t returnAddr = int3Addr;
WriteProcessMemory(pid, regs.rsp, &returnAddr, sizeof(returnAddr));
```

This ensures a clean return from dlopen to our trap instruction.
