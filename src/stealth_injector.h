#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace StealthInjector {

//=============================================================================
// Structures
//=============================================================================

struct ProcessInfo {
    uint32_t pid;
    std::string name;
    std::string path;
    bool isWine;           // Running under Wine/Proton
    uint32_t winePid;      // Wine server PID if applicable
    std::string winePrefix;
};

struct ModuleInfo {
    std::string name;
    uintptr_t base;
    size_t size;
    std::string path;
};

struct InjectionConfig {
    std::string libraryPath;
    std::string targetProcess;
    uint32_t targetPid = 0;
    bool verbose = true;
    bool antiDebug = true;      // Remove ptrace detection traces
    bool hideModule = true;     // Hide from /proc/modules
    bool spoofThreads = true;   // Restore thread states
    int injectionMethod = 0;    // 0=auto, 1=ptrace, 2=memfd, 3=preload
    std::string winePrefix;     // For Proton games
};

struct InjectionResult {
    bool success;
    std::string error;
    uintptr_t injectedBase;
    uint32_t injectedPid;
    std::vector<std::string> warnings;
};

//=============================================================================
// Stealth Injection Class
//=============================================================================

class StealthInjection {
public:
    StealthInjection();
    ~StealthInjection();
    
    // Process discovery
    std::vector<ProcessInfo> FindProcesses(const std::string& name);
    std::optional<ProcessInfo> FindProcess(const std::string& name);
    std::optional<ProcessInfo> FindProcessByPid(uint32_t pid);
    std::optional<ProcessInfo> FindWineProcess(const std::string& winePrefix, const std::string& processName);
    
    // Module operations
    std::vector<ModuleInfo> GetModules(uint32_t pid);
    std::optional<ModuleInfo> GetModule(uint32_t pid, const std::string& name);
    uintptr_t GetProcAddress(uint32_t pid, uintptr_t moduleBase, const std::string& symbol);
    
    // Injection methods
    InjectionResult Inject(const InjectionConfig& config);
    
private:
    // Low-level injection methods
    bool InjectPtrace(uint32_t pid, const std::string& libraryPath, InjectionResult& result);
    bool InjectMemfd(uint32_t pid, const std::vector<uint8_t>& libraryData, InjectionResult& result);
    bool InjectPreload(const std::string& libraryPath, const std::string& targetCmd);
    
    // Wine/Proton specific
    bool InjectWine(uint32_t pid, const std::string& libraryPath, InjectionResult& result);
    bool IsWineProcess(uint32_t pid);
    std::string GetWinePrefix(uint32_t pid);
    uint32_t GetWineServerPid(const std::string& winePrefix);
    
    // Stealth utilities (public for external use)
public:
    bool CheckAntiCheat(uint32_t pid);
    
private:
    void ClearPtraceTraces(uint32_t pid);
    void HideFromMaps(uint32_t pid, uintptr_t base);
    bool BypassTracerPid();
    
    // Memory operations
    bool ReadProcessMemory(uint32_t pid, uintptr_t address, void* buffer, size_t size);
    bool WriteProcessMemory(uint32_t pid, uintptr_t address, const void* buffer, size_t size);
    uintptr_t AllocateRemoteMemory(uint32_t pid, size_t size, int protection);
    bool CallRemoteFunction(uint32_t pid, uintptr_t function, uintptr_t* args, size_t argCount, uintptr_t* result);
    
    // Helper functions
    std::vector<uint32_t> GetThreads(uint32_t pid);
    std::vector<uint8_t> ReadLibraryFile(const std::string& path);
    std::string GenerateRandomName(size_t length);
};

//=============================================================================
// Global Functions
//=============================================================================

// Quick injection helper
bool QuickInject(const std::string& processName, const std::string& libraryPath);

// Proton-specific injection
bool InjectProton(const std::string& processName, const std::string& libraryPath, const std::string& protonPrefix = "");

} // namespace StealthInjector