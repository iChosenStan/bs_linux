# Blood Strike DLL - Extracted Offsets Guide

## Overview

This document explains the offsets extracted from the Windows `bloodstrike.dll` and how they relate to the Linux recreation.

## Two Types of Offsets

### 1. DLL Internal Offsets (INCLUDED in offsets.h)

These are offsets **within the cheat DLL itself** - they describe where the cheat stores its own variables and data:

| Variable | RVA | VA | Purpose |
|----------|-----|-----|---------|
| `g_Device` | 0xA5EC8 | 0x1800A5EC8 | ID3D11Device* pointer |
| `g_DeviceContext` | 0xA5ED0 | 0x1800A5ED0 | ID3D11DeviceContext* pointer |
| `g_SwapChain` | 0xA5ED8 | 0x1800A5ED8 | IDXGISwapChain* pointer |
| `g_OriginalPresent` | 0xA5EB8 | 0x1800A5EB8 | Original Present function pointer |
| `g_MenuToggle` | 0xA509E | 0x1800A509E | Menu visibility state |
| Entry Point | 0x5BBD4 | 0x18005BBD4 | DLL entry point |

**These offsets are now included in `src/offsets.h`**

### 2. DirectX VTable Offsets (INCLUDED in offsets.h)

These are standard DirectX 11 VTable offsets used for hooking:

| Method | VTable Offset |
|--------|---------------|
| **IDXGISwapChain::Present** | **0x140** (HOOK TARGET) |
| IDXGISwapChain::GetBuffer | 0x148 |
| IDXGISwapChain::ResizeBuffers | 0x138 |

**These offsets are now included in `src/offsets.h`**

### 3. Game Memory Offsets (NOT INCLUDED - PLACEHOLDERS)

These would be offsets **within the game's memory** to find:
- Entity list
- Player health
- Bone matrices
- View angles
- Weapon stats

**These offsets are NOT in the cheat DLL** - they must be found by:
1. Memory scanning the actual game process
2. Using a debugger to find variable addresses
3. Analyzing the game's memory patterns
4. Using existing offset databases for the specific game

The `offsets.h` file includes placeholder structures for these (`Offsets::Game` namespace).

## Files Updated

1. **`src/offsets.h`** (NEW) - Contains all extracted offsets:
   - DLL internal offsets
   - DirectX VTable offsets
   - Byte patterns and signatures
   - Feature string offsets
   - Hotkey values
   - Game offset placeholders

2. **`src/cheat_features.h`** - Updated to include offsets.h

3. **`src/cheat_features.cpp`** - Updated with comments referencing offsets

4. **`src/hooks.cpp`** - Updated with DirectX offset references

5. **`src/main.cpp`** - Updated with offset documentation in header

## How to Use

### Using DLL Internal Offsets

```cpp
#include "offsets.h"

// Access DirectX device pointer offset
uintptr_t deviceOffset = Offsets::DLL::G_DEVICE;

// Access VTable Present offset
uintptr_t presentVTable = Offsets::DirectX::VTABLE_PRESENT;
```

### Finding Game Offsets

You need to find game-specific offsets by:
1. Attaching a debugger to the game
2. Finding memory addresses for entities, health, etc.
3. Updating the `Offsets::Game` namespace with correct values

## Important Note

The Windows DLL was designed for DirectX 11. The Linux version uses Vulkan instead. The DirectX VTable offsets (like 0x140 for Present) are useful for understanding the original implementation but are not directly applicable to Linux Vulkan. Instead, we hook `vkQueuePresentKHR`.

## Byte Patterns

The offsets.h file includes byte patterns for signature scanning:

```cpp
// Entry Point Pattern at 0x18005BBD4
const char ENTRY_POINT[] = "\x48\x83\xEC\x28\xE8\xBB\xFF\xFF\xFF...";

// Present Hook Pattern at 0x1800011D0
const char PRESENT_HOOK[] = "\x40\x53\x55\x56\x48\x81\xEC\x90\x00\x00\x00...";
```

These can be used for pattern matching to find functions in memory.