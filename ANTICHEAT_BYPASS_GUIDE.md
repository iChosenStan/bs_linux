# Blood Strike Linux - Anti-Cheat Bypass Guide

## Overview

This guide documents the advanced anti-cheat bypass techniques implemented in this project, based on the HITB SecConf 2023 presentation **"Bypassing Anti-Cheats & Hacking Competitive Games"** by Rohan Aggarwal (@nahoragg).

## Table of Contents

1. [Anti-Cheat Detection Vectors](#detection-vectors)
2. [Cheat Types](#cheat-types)
3. [Kernel Driver Bypass](#kernel-driver-bypass)
4. [Hardware Bypass (Pixelbot)](#hardware-bypass)
5. [Linux-Specific Advantages](#linux-advantages)
6. [Implementation Details](#implementation)
7. [Usage Instructions](#usage)

---

## Detection Vectors

Modern kernel anti-cheats (Vanguard, BattlEye, EAC) detect cheats through:

| Detection Vector | Description | Bypass Method |
|-----------------|-------------|---------------|
| Process Handle Stripping | Block handle access to game process | Kernel driver / ptrace |
| Test Signing | Detect Windows test mode | Use signed driver / exploit |
| Usermode Hooks | Detect IAT/hooks in game | Manual mapping / inline hooks |
| Injected Modules | Detect DLL injection | Manual mapping / LD_PRELOAD |
| Manually Mapped Modules | Detect manually mapped DLLs | Clear traces (PiDDB, MmUnloadedDrivers) |
| Kernel Drivers | Detect loaded kernel drivers | Hide from driver lists |
| Driver Traces | Detect traces of unloaded drivers | Clear PiDDBCacheTable, MmUnloadedDrivers |
| VM Detection | Detect virtual machines | Patch VM checks / use bare metal |

---

## Cheat Types

### Internal Cheats
- **Injected into target process**
- More complex to develop
- Better performance
- Preferred for games with low-level or no AC

### External Cheats
- **Separate process manipulating target**
- Easier to develop (any language)
- Works with strong AC
- Uses process memory read/write

### Kernel Cheats
- **Custom kernel driver**
- Direct memory access
- Hardest to detect
- Requires driver signing or exploit

### Hardware Cheats
- **External hardware (Arduino)**
- Very hard to detect
- Works on almost all games
- Color-based detection

---

## Kernel Driver Bypass

### Driver Loading Methods

| Method | Description | Detection Risk |
|--------|-------------|----------------|
| Test Mode | Load unsigned driver in test mode | HIGH (detected by modern AC) |
| Signed Driver | Pay Microsoft for signing | MEDIUM (can be revoked) |
| Vulnerable Driver | Exploit signed driver | LOW (if driver is trusted) |
| Manual Mapping | KDMapper-style mapping | MEDIUM (traces must be cleared) |

### KDMapper
- Exploits `iqvw64e.sys` (Intel driver, CVE-2015-2291)
- Automatically clears: MmUnloadedDrivers, PiDDBCacheTable, g_KernelHashBucketList
- Does NOT clear: PoolBigPageTable

### Traces to Clear
```cpp
// Cleared by KDMapper:
MmUnloadedDrivers
PiDDBCacheTable
g_KernelHashBucketList

// NOT cleared by KDMapper:
PoolBigPageTable  // Must clear manually
```

### System Call Hooking
Avoid public syscalls that are signatured:
- ❌ `NtOpenCompositionSurfaceSectionInfo` (signatured)
- ✅ Find your own undocumented syscall

### Shellcode Patterns
**AVOID** (signatured):
```asm
mov rax, <address>
jmp rax
```

**USE** (mid-function hooking):
```asm
; Custom shellcode with relative jumps
call <relative_offset>
; ... your code
jmp <back_to_function>
```

---

## Hardware Bypass (Pixelbot)

### Pixelbot Generations

| Generation | Method | Detection Risk |
|------------|--------|----------------|
| Gen 1 | Software mouse (SendInput, SetCursorPos) | HIGH |
| Gen 2 | Driver-based (Interception, Razer drivers) | MEDIUM |
| Gen 3 | Arduino as second mouse | LOW |
| Gen 4 | Arduino + Hyper-V virtual mouse | VERY LOW |
| Gen 5 | Arduino + USB Host Shield (spoofed) | MINIMAL |

### How Pixelbots Work

```
1. Capture Screen → Get pixels from display
2. Color Filtering → Find enemy colors (HSV range)
3. Calculate Coordinates → Find enemy center
4. Send Coordinates → Move mouse to target
```

### Color Detection

Use HSV color space for better detection:

```cpp
struct HSVRange {
    int hMin, hMax;  // Hue (0-180)
    int sMin, sMax;  // Saturation (0-255)
    int vMin, vMax;  // Value (0-255)
};

// Example: Red enemy outlines
HSVRange redEnemy = {160, 180, 100, 255, 100, 255};
```

### Arduino Configuration

**Hardware Required:**
- Arduino Leonardo (ATmega32U4)
- USB Host Shield (optional, for Gen 5)

**Communication Methods:**
- Serial (monitored by some AC)
- Web server
- Wireless transmitter

**Spoofing Arduino VID/PID:**
Edit `boards.txt` to make Arduino appear as real mouse:
```
leonardo.vid.2=0x046D  // Logitech
leonardo.pid.2=0xC52B  // G502
```

Find VID/PID at: https://the-sz.com/products/usbid/index.php

### Arduino Commands

```
M<dx>,<dy>  - Move mouse by dx, dy pixels
C           - Left click
R           - Right click
S           - Status
```

---

## Linux Advantages

### Anti-Cheat Support

| Anti-Cheat | Linux Support | Notes |
|------------|---------------|-------|
| BattlEye | ✅ Supported | Native Linux driver |
| EAC | ✅ Supported | Native Linux driver |
| Vanguard | ❌ Not Supported | Won't run on Linux |

### Linux Detection Evasion

**What Anti-Cheats CAN Detect:**
- Process names (cheat processes)
- Loaded libraries (LD_PRELOAD)
- Debuggers (ptrace attachment)
- Modified game files

**What Anti-Cheats CAN'T Easily Detect:**
- Kernel modules (no kernel AC for most games)
- External hardware (same as Windows)
- Wine/Proton internals

### Memory Access Methods

```cpp
// Method 1: /proc/<pid>/mem
int fd = open("/proc/1234/mem", O_RDWR);
pread(fd, buffer, size, address);

// Method 2: process_vm_readv/writev
process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);

// Method 3: ptrace
ptrace(PTRACE_ATTACH, pid, nullptr, nullptr);
ptrace(PTRACE_PEEKDATA, pid, address, nullptr);
```

---

## Implementation

### Project Structure

```
bloodstrike_linux/
├── src/
│   ├── anticheat_bypass.h      # Anti-cheat bypass techniques
│   ├── pixelbot.h              # Hardware aimbot header
│   ├── pixelbot.cpp            # Hardware aimbot implementation
│   ├── offsets.h               # Extracted offsets from DLL
│   ├── cheat_features.h/cpp    # Main cheat features
│   ├── hooks.h/cpp             # Hooking system
│   └── main.cpp                # Entry point
├── arduino/
│   └── pixelbot_arduino/
│       └── pixelbot_arduino.ino  # Arduino sketch
├── CMakeLists.txt
├── README.md
├── OFFSETS_GUIDE.md
└── ANTICHEAT_BYPASS_GUIDE.md   # This file
```

### Building

```bash
cd bloodstrike_linux
mkdir build && cd build
cmake ..
make
```

### Using Pixelbot

**Software Mouse (Gen 1):**
```cpp
#include "pixelbot.h"

Pixelbot::Aimbot aimbot;
aimbot.initialize();

Pixelbot::AimbotSettings settings;
settings.enabled = true;
settings.targetColor = Pixelbot::EnemyColors::BLOODSTRIKE_ENEMY;
settings.fovDegrees = 90.0f;
settings.smoothness = 5.0f;

aimbot.setSettings(settings);

// In main loop:
aimbot.update();
```

**Arduino Mouse (Gen 3-5):**
```cpp
Pixelbot::MouseController mouse;
mouse.setGeneration(Pixelbot::MouseGen::GEN_3_ARDUINO);
mouse.connectArduino("/dev/ttyACM0", 115200);

// Send movement
mouse.sendArduinoMove(10, -5);
```

### Arduino Setup

1. Install Arduino IDE
2. Install Arduino Leonardo board
3. Upload `pixelbot_arduino.ino`
4. Connect to Linux via USB
5. Use `/dev/ttyACM0` (or similar) for communication

---

## Usage

### Method 1: LD_PRELOAD (Recommended for Linux)

```bash
LD_PRELOAD=./libbloodstrike.so ./game
```

### Method 2: Injection

```bash
# Find game PID
pgrep game_name

# Inject
./injector <pid> ./libbloodstrike.so
```

### Method 3: Pixelbot (Hardware)

```bash
# Connect Arduino
ls /dev/ttyACM*

# Run pixelbot
./bloodstrike_pixelbot --arduino /dev/ttyACM0
```

---

## Advanced Techniques

### String Obfuscation

Avoid signature detection by obfuscating strings:

```cpp
// Instead of:
const char* str = "aimbot";

// Use:
#define OBFUSCATE(str, key) AntiCheat::Evasion::ObfuscatedString(str, key).decrypt(key)
const char* str = OBFUSCATE("aimbot", 0x42);
```

### YOLO AI Aimbot

For games without enemy outlines:

```cpp
#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

Pixelbot::YOLODetector detector;
detector.loadModel("yolov4.weights", "yolov4.cfg");

std::vector<Target> targets = detector.detect(frame, 0.5f);
#endif
```

### Offset Finding

**Methods:**
1. Debugging (Cheat Engine, x64dbg)
2. Disassembly (IDA Pro, Ghidra)
3. Dumps (dumps.host)
4. Community (unknowncheats.me)
5. Pattern scanning

**Resources:**
- https://dumps.host/
- https://www.unknowncheats.me/
- https://github.com/

---

## EAC (Easy Anti-Cheat) Specific Bypass

*Based on GH EAC Bypass Guide (credits: @iPower & adrianyy)*

### Games Using EAC

- Apex Legends
- Fortnite
- Rust
- Paladins
- Dead by Daylight
- For Honor
- Gears 5
- The Division 2
- And many more

### EAC Detection Capabilities

EAC is the 2nd most powerful kernel anticheat. It can detect:

- Block all interaction with game process
- Block creation of process handles
- Scan for hidden processes & modules
- Scan for known suspicious DLL modules
- Scan for known suspicious drivers
- Get a list of all open handles
- Scan for disks & devices
- Log all loaded drivers
- Gather HWID information
- Detect debuggers
- Find manually mapped drivers
- Detect manually mapped driver traces
- Check for kernel patches
- Find handles to physical memory
- Detect modules using VirtualProtect
- Dump suspect strings from regions not backed by modules
- Scan for syscall stubs in non-module regions
- Window enumeration for suspect overlays
- Enumerate suspect shared memory sections
- Detect hooks
- Check all services
- Scan all threads & system threads
- Stack walking
- Turla Driver Loader detection
- Hypervisor & VM detection
- DbgUiRemoteBreakin patch
- PsGetProcessDebugPort
- HideFromDebugger flag detection
- Debug register reads (DR6, DR7)
- Instrumentation callbacks

### Manually Mapped Driver Detection Bypass

You must solve these 4 things:

1. **PiDDBCacheTable & MmUnloadedDrivers** - Clear driver traces
2. **System pool detection** - Avoid suspicious pool allocations
3. **System thread detection** - Hide threads properly
4. **Driver object cleanup** - Remove driver references

### EAC HWID Generation Sources

EAC gathers hardware info from:

- `KUSER_SHARED_DATA.ProcessorFeatures` (0xFFFFF78000000274)
- Registry keys (see below)
- WMI queries
- Ntoskrnl.exe version
- MAC address
- Disk serials

#### Registry Keys for HWID

| Key Path | Information |
|----------|-------------|
| `HKLM\Hardware\Description\System\CentralProcessor\0` | CPU info, SystemProductName |
| `HKLM\HARDWARE\DEVICEMAP\Scsi\Scsi Port 0\Scsi Bus 0\Target Id 0\Logical Unit Id 0` | Disk Identifier, SerialNumber |
| `HKLM\System\CurrentControlSet\Control\SystemInformation` | ComputerHardwareId |
| `HKLM\HARDWARE\DESCRIPTION\System\BIOS` | BIOSVendor, BIOSReleaseDate, ProductId |
| `HKLM\System\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000` | GPU info |
| `HKLM\System\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}\0001` | Network adapter |
| `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion` | InstallDate, DriverDesc |
| `HKLM\Software\Microsoft\Windows\CurrentVersion\WindowsUpdate` | SusClientId |

#### Linux HWID Equivalent Paths

| Windows | Linux |
|---------|-------|
| Product UUID | `/sys/class/dmi/id/product_uuid` |
| Board Serial | `/sys/class/dmi/id/board_serial` |
| BIOS Vendor | `/sys/class/dmi/id/bios_vendor` |
| CPU Info | `/proc/cpuinfo` |
| MAC Address | `/sys/class/net/*/address` |
| Disk Serial | `/sys/block/*/serial` |

### EAC Bypass Requirements

- **Kernel driver required** - EAC requires kernel-level bypass
- **Load driver first** - Load BEFORE EAC starts
- **HWID spoofing** - Usually required for banned users
- **Trace clearing** - Clear PiDDBCacheTable & MmUnloadedDrivers
- **Pool cleaning** - Clean system pool traces
- **Thread hiding** - Hide system threads

---

## Resources

### From Presentation

- Kernel Driver: https://github.com/nahoragg/KernelDriver
- Arduino: https://github.com/nahoragg/Arduino

### General Resources

- Guided Hacking: https://guidedhacking.com/
- UnknownCheats: https://www.unknowncheats.me/
- KDMapper: https://github.com/TheCruZ/kdmapper
- KDU: https://github.com/hfiref0x/KDU
- Screwed Drivers: https://github.com/eclypsium/Screwed-Drivers

### EAC Bypass References

- GH EAC Bypass Guide (credits: @iPower & adrianyy)
- https://guidedhacking.com/threads/eac-bypass.14897/

### YOLO AI Aimbot

https://www.unknowncheats.me/forum/general-programming-and-reversing/485725-guide-ai-aimbot.html

---

## Disclaimer

This project is for educational purposes only. Using cheats in competitive games violates terms of service and can result in permanent bans. The techniques described here should only be used for:
- Security research
- Anti-cheat development
- Educational purposes

---

## Credits

- **Rohan Aggarwal (@nahoragg)** - HITB SecConf 2023 Presentation
- **@iPower & adrianyy** - EAC Bypass Guide
- **Original Blood Strike DLL** - Reverse engineered for offsets
- **KDMapper** - Driver mapping tool
- **Dear ImGui** - GUI framework
- **Vulkan** - Graphics API

---

## License

This project is provided as-is for educational purposes.