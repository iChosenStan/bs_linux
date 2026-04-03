#pragma once

//=============================================================================
// ANTI-CHEAT BYPASS TECHNIQUES
// Based on HITB SecConf 2023 - "Bypassing Anti-Cheats & Hacking Competitive Games"
// by Rohan Aggarwal (@nahoragg)
//=============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace AntiCheat {

//=============================================================================
// KERNEL ANTI-CHEAT FEATURES TO BYPASS
//=============================================================================
// Modern kernel anti-cheats (like Vanguard, BattlEye, EAC) implement:
//
// 1. Blocking/stripping of process handles in User Mode
// 2. Detection of test signing
// 3. Detection of usermode hooks
// 4. Detection of injected modules
// 5. Detection of manually mapped modules
// 6. Detection of kernel drivers
// 7. Detection of traces of manually mapped drivers
// 8. Detection of virtual machines and emulation
//=============================================================================

namespace Detection {
    // What anti-cheats look for
    enum class DetectionVector {
        PROCESS_HANDLE_STRIPPING,   // Block handle access
        TEST_SIGNING,               // Test mode detection
        USERMODE_HOOKS,             // IAT/hooks detection
        INJECTED_MODULES,           // DLL injection detection
        MANUALLY_MAPPED_MODULES,    // Manual map detection
        KERNEL_DRIVERS,             // Driver detection
        DRIVER_TRACES,              // PiDDBCacheTable, MmUnloadedDrivers
        VM_DETECTION,               // Virtual machine detection
        POOL_BIG_PAGE_TABLE,        // PoolBigPageTable traces
    };

    struct DetectionInfo {
        DetectionVector vector;
        std::string description;
        std::string bypassMethod;
    };
}

//=============================================================================
// CHEAT TYPES
//=============================================================================
namespace CheatType {
    // Internal: Injected into target process
    // - More complex to make
    // - Better performance
    // - Preferred for games with low-level or no AC
    
    // External: Separate process manipulating target
    // - Easier to make
    // - Works with any language
    // - Preferred for games with strong AC
    
    enum class Type {
        INTERNAL,       // Injected into game process
        EXTERNAL,       // Separate process
        KERNEL,         // Kernel driver
        HARDWARE        // External hardware (Arduino)
    };
}

//=============================================================================
// KERNEL DRIVER BYPASS TECHNIQUES
//=============================================================================
namespace KernelBypass {

// Driver loading methods
enum class LoadMethod {
    TEST_MODE,          // Only for testing, detected by modern AC
    SIGNED_DRIVER,      // Pay Microsoft for signing (can be revoked)
    VULNERABLE_DRIVER,  // Exploit signed driver to load unsigned
    MANUAL_MAP          // KDMapper-style manual mapping
};

// System call hooking targets
// Avoid public syscalls that are signatured
struct SystemCallHook {
    std::string moduleName;     // e.g., "dxgkrnl.sys"
    std::string functionName;   // e.g., "NtOpenCompositionSurfaceSectionInfo"
    bool isSignatured;          // Is this signatured by AC?
};

// Traces to clear after manual mapping
namespace Traces {
    // KDMapper clears these automatically:
    constexpr const char* MM_UNLOADED_DRIVERS = "MmUnloadedDrivers";
    constexpr const char* PIDDB_CACHE_TABLE = "PiDDBCacheTable";
    constexpr const char* KERNEL_HASH_BUCKET_LIST = "g_KernelHashBucketList";
    
    // Not cleared by KDMapper:
    constexpr const char* POOL_BIG_PAGE_TABLE = "PoolBigPageTable";
}

// Shellcode patterns to avoid (signatured)
namespace ShellcodePatterns {
    // AVOID: Classic hook shellcode (detected)
    // mov rax, <address>
    // jmp rax
    // 
    // Bytes: 48 B8 xx xx xx xx xx xx xx xx FF E0
    
    // BETTER: Mid-function hooking with custom shellcode
    // Use relative jumps, indirect calls, etc.
}

} // namespace KernelBypass

//=============================================================================
// HARDWARE BYPASS (Arduino / External)
//=============================================================================
namespace HardwareBypass {

// Pixelbot generations (evolution of mouse movement)
enum class PixelbotGen {
    GEN_1,  // Software mouse: pyautogui, SendInput, SetCursorPos
    GEN_2,  // Driver-based: Interception, Razer/Logitech drivers
    GEN_3,  // Arduino as second mouse
    GEN_4,  // Arduino + Hyper-V virtual mouse
    GEN_5   // Arduino + USB Host Shield (spoofed real mouse)
};

// Arduino configuration
struct ArduinoConfig {
    std::string comPort;
    int baudRate;
    bool useUSBHostShield;
    bool spoofVIDPID;           // Spoof Arduino VID/PID
    uint16_t vendorId;          // Spoofed vendor ID
    uint16_t productId;         // Spoofed product ID
};

// Communication methods
enum class CommMethod {
    SERIAL,             // Serial communication (monitored by some AC)
    WEB_SERVER,         // HTTP-based communication
    WIRELESS            // Wireless transmitter
};

// Pixelbot pipeline
namespace Pixelbot {
    // 1. Capture - Get pixels from screen
    // 2. Color Filtering - Find enemy colors (HSV range)
    // 3. Calculate Coordinates - Find enemy center
    // 4. Send Coordinates - Move mouse to target
    
    struct ColorRange {
        int hMin, hMax;  // Hue range
        int sMin, sMax;  // Saturation range
        int vMin, vMax;  // Value range
    };
    
    // Common enemy outline colors (game-specific)
    // Need to calibrate per game using OpenCV
}

} // namespace HardwareBypass

//=============================================================================
// OFFSET FINDING TECHNIQUES
//=============================================================================
namespace OffsetFinding {

// Methods to find game offsets
enum class Method {
    DEBUGGING,          // Cheat Engine, x64dbg
    DISASSEMBLY,        // IDA Pro, Ghidra
    DUMPS,              // dumps.host
    COMMUNITY,          // unknowncheats.me, GitHub
    PATTERN_SCAN        // Signature scanning
};

// Offset structure
struct Offset {
    std::string name;
    uintptr_t address;
    std::vector<uintptr_t> offsets;  // Multi-level pointer
    std::string pattern;
    std::string mask;
};

// Common offset patterns
// struct enemy {
//     int health;   // offset 0x0
//     int ammo;     // offset 0x4
//     float x;      // offset 0x8
//     float y;      // offset 0xC
//     float z;      // offset 0x10
// };

} // namespace OffsetFinding

//=============================================================================
// LINUX-SPECIFIC BYPASS TECHNIQUES
//=============================================================================
namespace LinuxBypass {

// Linux doesn't have kernel AC in the same way Windows does
// Most competitive games on Linux run via:
// 1. Native Linux ports (rare for competitive games)
// 2. Wine/Proton (Windows compatibility layer)
// 3. Steam Deck (Valve's Proton-based system)

// Anti-cheat support on Linux:
// - BattlEye: Supported via Proton (with native Linux driver)
// - EAC: Supported via Proton (with native Linux driver)
// - Vanguard: NOT supported on Linux

enum class LinuxACSupport {
    SUPPORTED,          // Anti-cheat works on Linux
    PARTIAL,            // Works with Proton but may have issues
    NOT_SUPPORTED,      // Game won't run on Linux
    BYPASSED            // Anti-cheat effectively bypassed by Linux
};

// Process memory access on Linux
namespace MemoryAccess {
    // Methods for reading/writing game memory on Linux
    enum class Method {
        PROC_MEM,           // /proc/<pid>/mem
        PROCESS_VM,         // process_vm_readv/writev
        PTRACE,             // ptrace syscall
        SHARED_MEMORY,      // mmap shared memory
        KERNEL_MODULE       // Custom kernel module
    };
    
    // /proc/<pid>/maps - Find memory regions
    // /proc/<pid>/mem - Read/write memory
    // process_vm_readv/writev - Cross-process memory access
}

// Detection vectors on Linux
namespace Detection {
    // What anti-cheats CAN detect on Linux:
    // 1. Process names (cheat processes)
    // 2. Loaded libraries (LD_PRELOAD)
    // 3. Debuggers (ptrace attachment)
    // 4. Modified game files
    
    // What anti-cheats CAN'T easily detect on Linux:
    // 1. Kernel modules (no kernel AC for most games)
    // 2. External hardware (same as Windows)
    // 3. Wine/Proton internals
}

// Bypass advantages on Linux
constexpr bool NO_KERNEL_AC = true;         // Most games have no kernel AC
constexpr bool PROC_MEM_ACCESS = true;      // Easy process memory access
constexpr bool PTRACE_WORKS = true;         // ptrace usually not blocked
constexpr bool LD_PRELOAD_WORKS = true;     // LD_PRELOAD injection works

} // namespace LinuxBypass

//=============================================================================
// YOLO AI AIMBOT
//=============================================================================
namespace AIAimbot {
    // YOLO (You Only Look Once) for enemy detection
    // More advanced than color-based pixelbots
    // 
    // Advantages:
    // - Works without enemy outlines
    // - Can detect multiple enemies
    // - Can predict movement
    // - Harder to detect than memory-based cheats
    
    // Resources:
    // https://www.unknowncheats.me/forum/general-programming-and-reversing/485725-guide-ai-aimbot.html
}

//=============================================================================
// DETECTION EVASION
//=============================================================================
namespace Evasion {

// String obfuscation to avoid signature detection
template<size_t N>
class ObfuscatedString {
private:
    std::array<char, N> encrypted;
    std::array<char, N> decrypted;
    bool isDecrypted = false;
    
public:
    constexpr ObfuscatedString(const char(&str)[N], char key) {
        for (size_t i = 0; i < N; i++) {
            encrypted[i] = str[i] ^ key;
        }
    }
    
    const char* decrypt(char key) {
        if (!isDecrypted) {
            for (size_t i = 0; i < N; i++) {
                decrypted[i] = encrypted[i] ^ key;
            }
            isDecrypted = true;
        }
        return decrypted.data();
    }
};

// Simple XOR encryption for strings
#define OBFUSCATE(str, key) AntiCheat::Evasion::ObfuscatedString(str, key).decrypt(key)

// Avoid common detected patterns
namespace Patterns {
    // Don't use:
    // - "aimbot" in strings
    // - "ESP" in strings
    // - "hack" in strings
    // - Standard hook byte patterns
    // - Known module names
}

} // namespace Evasion

//=============================================================================
// VULNERABLE DRIVERS FOR EXPLOITATION
//=============================================================================
namespace VulnDrivers {
    // KDMapper uses: iqvw64e.sys (Intel Network Adapter driver)
    // CVE-2015-2291
    
    // Other vulnerable drivers:
    // - https://github.com/hfiref0x/KDU
    // - https://github.com/eclypsium/Screwed-Drivers
    // - https://guidedhacking.com/threads/vulnerable-kernel-drivers-for-exploitation.15979/
    
    // Note: These are Windows-only, not applicable to Linux
}

//=============================================================================
// EAC (EASY ANTI-CHEAT) SPECIFIC BYPASS
// Based on GH EAC Bypass Guide
//=============================================================================
namespace EAC {

// Games using EAC:
// - Apex Legends
// - Fortnite
// - Rust
// - Paladins
// - Dead by Daylight
// - For Honor
// - Gears 5
// - The Division 2
// - And many more

// EAC is the 2nd most powerful kernel anticheat (after Vanguard)
// You MUST load your kernel driver BEFORE the anticheat starts

namespace DetectionCapabilities {
    // EAC can detect ALL of these:
    // - Block all interaction with game process
    // - Block creation of process handles
    // - Scan for hidden processes & modules
    // - Scan for known suspicious DLL modules
    // - Scan for known suspicious drivers
    // - Get a list of all open handles
    // - Scan for disks & devices
    // - Log all loaded drivers
    // - Gather HWID information
    // - Detect debuggers
    // - Find manually mapped drivers
    // - Detect manually mapped driver traces
    // - Check for kernel patches
    // - Find handles to physical memory
    // - Detect modules using VirtualProtect
    // - Dumps suspect strings from regions not backed by actual modules
    // - Scans for possible syscall stubs in regions not backed by modules
    // - Window enumeration to detect suspect overlays
    // - Enumerates suspect shared memory sections
    // - Detect hooks
    // - Checks all services
    // - Scan all threads & system threads
    // - Stack walking
    // - Turla Driver Loader detection
    // - Hypervisor & VM detection
    // - DbgUiRemoteBreakin patch
    // - PsGetProcessDebugPort
    // - Set HideFromDebugger flag manually
    // - Reads DR6 and DR7 (debug registers)
    // - Instrumentation callbacks
}

// Manually mapped driver detection bypass
namespace ManualMapBypass {
    // You must solve these 4 things:
    // 1. PiDDBCacheTable & MmUnloadedDrivers - clear traces
    // 2. System pool detection - avoid suspicious pool allocations
    // 3. System thread detection - hide threads properly
    // 4. Driver object cleanup - remove driver references
    
    // KDMapper handles most of these automatically
    constexpr bool CLEAR_PIDDB_CACHE = true;
    constexpr bool CLEAR_MMUNLOADED_DRIVERS = true;
    constexpr bool CLEAN_SYSTEM_POOL = true;
    constexpr bool HIDE_SYSTEM_THREADS = true;
}

// EAC HWID (Hardware ID) Generation Sources
// EAC knows exactly who you are - spoofing is necessary
namespace HWIDSources {
    // Primary sources:
    // - KUSER_SHARED_DATA.ProcessorFeatures (0xFFFFF78000000274)
    // - Registry keys (see below)
    // - WMI queries
    // - Ntoskrnl.exe version
    // - MAC address
    // - Disk serials
    
    // Registry keys used for HWID:
    namespace RegistryKeys {
        // CPU/Motherboard info
        constexpr const char* CPU_INFO = 
            "HKEY_LOCAL_MACHINE\\Hardware\\Description\\System\\CentralProcessor\\0";
        constexpr const char* SYSTEM_PRODUCT = 
            "HKEY_LOCAL_MACHINE\\Hardware\\Description\\System\\CentralProcessor\\0\\SystemProductName";
        
        // Disk info
        constexpr const char* DISK_SERIAL = 
            "HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port 0\\Scsi Bus 0\\"
            "Target Id 0\\Logical Unit Id 0\\IdentifierSerialNumber";
        
        // System info
        constexpr const char* SYSTEM_INFO = 
            "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\SystemInformation\\"
            "ComputerHardwareId";
        
        // BIOS info
        constexpr const char* BIOS_INFO = 
            "HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS";
        // Sub-keys: BIOSVendor, BIOSReleaseDate, ProductId, ProcessorNameString
        
        // GPU info
        constexpr const char* GPU_INFO = 
            "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Class\\"
            "{4d36e968-e325-11ce-bfc1-08002be10318}\\0000";
        
        // Network adapter
        constexpr const char* NETWORK_ADAPTER = 
            "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Class\\"
            "{4d36e972-e325-11ce-bfc1-08002be10318}\\0001";
        
        // Windows installation
        constexpr const char* WINDOWS_INSTALL = 
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        // Sub-keys: InstallDate, DriverDesc
        
        // Windows Update
        constexpr const char* WINDOWS_UPDATE = 
            "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\SusClientId";
    }
    
    // Linux equivalents for HWID spoofing
    namespace LinuxHWID {
        // On Linux, these are typically:
        // - /sys/class/dmi/id/product_uuid
        // - /sys/class/net/*/address (MAC)
        // - /sys/block/*/serial (disk serials)
        // - /proc/cpuinfo (CPU info)
        // - dmidecode output
        
        constexpr const char* PRODUCT_UUID = "/sys/class/dmi/id/product_uuid";
        constexpr const char* BOARD_SERIAL = "/sys/class/dmi/id/board_serial";
        constexpr const char* BIOS_VENDOR = "/sys/class/dmi/id/bios_vendor";
        constexpr const char* CPUINFO = "/proc/cpuinfo";
    }
}

// EAC bypass requirements
struct BypassRequirements {
    bool needsKernelDriver = true;      // EAC requires kernel-level bypass
    bool needsDriverFirst = true;       // Load driver BEFORE EAC starts
    bool needsHWIDSpoof = true;         // HWID spoofing usually required
    bool needsTraceClearing = true;     // Clear PiDDBCacheTable & MmUnloadedDrivers
    bool needsPoolCleaning = true;      // Clean system pool traces
    bool needsThreadHiding = true;      // Hide system threads
};

} // namespace EAC

//=============================================================================
// RESOURCES
//=============================================================================
namespace Resources {
    constexpr const char* GUIDED_HACKING = "https://guidedhacking.com/";
    constexpr const char* UNKNOWNCHEATS = "https://www.unknowncheats.me/";
    constexpr const char* DUMPS_HOST = "https://dumps.host/";
    constexpr const char* KDMAPPER = "https://github.com/TheCruZ/kdmapper";
    constexpr const char* KDU = "https://github.com/hfiref0x/KDU";
    
    // From HITB 2023 presentation:
    constexpr const char* KERNEL_DRIVER_REPO = "https://github.com/nahoragg/KernelDriver";
    constexpr const char* ARDUINO_REPO = "https://github.com/nahoragg/Arduino";
    
    // EAC bypass references:
    // - GH EAC Bypass Guide (credits: @iPower & adrianyy)
    // - https://guidedhacking.com/threads/eac-bypass.14897/
}

} // namespace AntiCheat