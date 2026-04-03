# BloodStrike Linux – Full Project Audit

## Files Audited
All 32 source files in `/src/`, `server/bridge.js`, `CMakeLists.txt`

---

## CRITICAL (crash / prevent injection)

### 1. `injector.cpp` – AllocateMemory: double-attach + broken POKETEXT + bad fd
- Line 208: Calls `Attach(tid)` while InjectLibrary may already hold the attach
- Line 247: `ptrace(PTRACE_POKETEXT, tid, (void*)codeAddr, (void*)0x050F)` — writes pointer VALUE 0x050F as 8 bytes → zeroes bytes 2–7, corrupts 6 bytes of code permanently (origWord restore only fixes 2 bytes)
- Line 227: `regs.r8 = 0xFFFFFFFF` (unsigned) → kernel sees a huge positive fd, returns EBADF
- Line 254: `PTRACE_SINGLESTEP` used instead of int3 trap — unreliable on modern ASLR kernels
- **NOTE**: `InjectLibrary()` always returns false anyway (line 352). But the code corruption still happens if it's called.

### 2. `stealth_injector.cpp` – BypassTracerPid() creates zombie child
- Line 812–817: `fork()` child exits immediately, parent never calls `waitpid()` → zombie process leaks

### 3. `hooks.cpp` – InlineHook::Remove() null check is broken
- Line 117: `if (!m_originalBytes[0])` — checks if first byte is NUL, which is valid (e.g. MOV prefix). Should be `if (!m_target || m_hookSize == 0)`

### 4. `hooks.cpp` – mprotect() covers only one page
- Line 91: Hook write spans 14 bytes but mprotect only covers one page. If hook straddles a page boundary, memcpy at line 108 → SIGSEGV
- Fix: `mprotect(pageStart, pageSize * 2, ...)`

### 5. `entity_manager.cpp` – ReadString() buffer overrun
- Line 71: `len < maxSize` should be `len < maxSize - 1`; `buffer[len] = '\0'` overwrites one byte past end when `len == maxSize - 1`

### 6. `main.cpp` – argv[3] out-of-bounds
- Line 238: Guard is `argc >= 3` but line 239 accesses `argv[3]` — UB crash when exactly 3 args given
- Fix: `argc >= 4`

---

## HIGH (incorrect behavior / silent failures)

### 7. `stealth_injector.cpp` – GetProcAddress() wrong for PIE shared libs
- Line 565: `return moduleBase + sym.st_value` — for PIE libs `st_value` is already an absolute VA. Adding moduleBase doubles it
- Fix: `return (sym.st_value < moduleBase) ? moduleBase + sym.st_value : sym.st_value`

### 8. `stealth_injector.cpp` – CheckAntiCheat() false-positive on oleacc.dll
- `name.find("eac")` matches "ol**eac**c.dll" (Windows accessibility DLL in Wine), triggering false AC warning every run
- Fix: Use `name == "eac.dll"` or full string `name.find("easyanticheat") != npos`

### 9. `bridge.js` – Shell arguments unquoted (command injection / space splitting)
- Line 45: Path variables not quoted in template literal command — breaks if paths contain spaces
- Fix: Quote all: `"${INJECTOR_PATH}"`, `"${LIB_PATH}"`, `"${target}"`

### 10. `bridge.js` – SELinux status always hardcoded "PERMISSIVE"
- Line 31: `seLinux: "PERMISSIVE"` is a mock. On Fedora with SELinux enforcing, ptrace is blocked but UI shows green
- Fix: `execSync('getenforce').toString().trim()`

### 11. `aimbot.cpp` – NormalizeAngle() infinite loop on NaN/Inf
- Lines 94–99: `while` loops never terminate on NaN
- Fix: Use `fmodf`-based one-liner normalization

### 12. `aimbot.cpp` – CalcAngle() axis mapping wrong for UE4
- Line 79–80: Uses `delta.y` as pitch and `atan2(x, z)` as yaw — but UE4 uses Y-forward/Z-up
- Correct: `pitch = asinf(-delta.z / hyp)`, `yaw = atan2f(delta.y, delta.x)`

### 13. `cheat_features.cpp` – DrawHealthBar() color byte order wrong
- Line 187: `0xFF0000FF` intended as red background — in RGBA this is blue (#0000FF), not red
- Fix (ABGR): Background=`0xFF0000FF` → red, Health=`0xFF00FF00` → green, Low=`0xFF00FFFF` → yellow

### 14. `entity_manager.cpp` – atoi() for PID (fragile)
- Line 141: `atoi()` returns 0 on failure (no error) — works but `strtol` with end-pointer check is correct

---

## MEDIUM (quality / edge cases)

### 15. `stealth_injector.cpp` – GetThreads() doesn't filter '.' and '..'  
- DT_UNKNOWN filesystems bypass the DT_DIR check; stoul catches it but better to pre-filter

### 16. `stealth_injector.cpp` – FindProcesses() leaks taskDir immediately (opens + closes with no reads)
- Lines 183–186: Opens dir, closes it, accomplishes nothing

### 17. `stealth_injector.cpp` – FindProcesses() sandbox search includes all ppid==1 processes
- Line 211: `ppid == 1` matches ALL init children (systemd, dbus, etc.) polluting results

### 18. `aimbot.cpp` – Screen size hardcoded 1920x1080
- Lines 388, 398, 484–489: Wrong on any other resolution — should query active display

### 19. `hooks.cpp` – No minimum function size check before 14-byte hook
- Hooking tiny functions corrupts adjacent ones

### 20. `entity_manager.cpp` – localPlayerAddr and viewMatrixAddr never set
- Always 0 → ReadLocalPlayer() and ReadViewMatrix() always return false

### 21. `CMakeLists.txt` – find_package(PkgConfig) called twice (lines 9, 100)
### 22. `CMakeLists.txt` – Release strip skips bloodstrike_loader target
### 23. `cheat_features.cpp` – #include inside namespace body (lines 254–256)

---

## LOW (cleanup)

### 24. `SplitString()` defined in both injector.cpp and stealth_injector.cpp, never used in stealth_injector
### 25. `aimbot.cpp` ApplyRecoilControl() always uses weapon ID 0 (hardcoded)
### 26. `bridge.js` no subprocess timeout — injector hang = bridge hangs forever
### 27. `main.cpp` signal.h included mid-file (line 75)
### 28. `ReadLibraryFile()` — no check that `file.read()` succeeded

---

## Already Fixed This Session
- [x] stealth_injector: double-attach deadlock → AllocateRemoteMemoryInProcess()
- [x] stealth_injector: mmap fd 0xFFFFFFFF → (unsigned long)(long)-1
- [x] stealth_injector: Wine address space selection
- [x] stealth_injector: WIFSTOPPED validation after PTRACE_ATTACH
