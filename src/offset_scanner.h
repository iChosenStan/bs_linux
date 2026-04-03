#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <regex>

namespace OffsetScanner {

// ============================================================================
// Pattern Scanning Structures
// ============================================================================

/**
 * Represents a byte pattern for signature scanning
 * Supports wildcards (??) and masked bytes
 */
struct BytePattern {
    std::vector<uint8_t> bytes;
    std::vector<bool> mask;      // true = check this byte, false = wildcard
    std::string patternStr;        // Original pattern string (e.g., "48 89 5C 24 ?? 48 89 74 24")
    std::string moduleName;        // Module to scan in (empty = main module)
    int32_t offset = 0;            // Offset from pattern match to add
    std::string name;              // Human-readable name for this pattern
};

/**
 * Result of a pattern scan
 */
struct ScanResult {
    bool found = false;
    uintptr_t address = 0;
    uintptr_t offset = 0;
    std::string name;
    std::string error;
    std::string moduleName;
    size_t matchCount = 0;
};

/**
 * Represents a discovered offset/structure
 */
struct DiscoveredOffset {
    std::string name;
    uintptr_t offset;
    uint32_t size;
    std::string typeName;
    std::string description;
    bool verified = false;
    std::string signature;         // Pattern used to find this offset
};

/**
 * Represents a discovered structure
 */
struct DiscoveredStruct {
    std::string name;
    uint32_t size;
    std::vector<DiscoveredOffset> members;
    uintptr_t patternAddress = 0;  // Where the struct pattern was found
    bool isComplete = false;
};

// ============================================================================
// Debugger Structures
// ============================================================================

/**
 * Breakpoint types
 */
enum class BreakpointType {
    EXECUTE,        // Software breakpoint (INT3)
    READ,           // Hardware breakpoint (read)
    WRITE,          // Hardware breakpoint (write)
    ACCESS          // Hardware breakpoint (read/write)
};

/**
 * Breakpoint information
 */
struct Breakpoint {
    uintptr_t address;
    BreakpointType type;
    uint8_t originalByte;
    int drIndex = -1;              // Debug register index (0-3) for hardware BP
    bool enabled = false;
    std::string description;
    int hitCount = 0;
};

/**
 * Register state for debugging
 */
struct RegisterState {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;
    uint64_t cs, ds, es, fs, gs, ss;
    
    // Floating point / SIMD
    __int128 xmm[16];
    uint16_t fpctrl, fpstatus;
    uint8_t fpregs[8][10];
};

/**
 * Debug event types
 */
enum class DebugEventType {
    BREAKPOINT,
    SINGLE_STEP,
    EXCEPTION,
    PROCESS_EXIT,
    THREAD_EXIT,
    MODULE_LOAD,
    MODULE_UNLOAD
};

/**
 * Debug event information
 */
struct DebugEvent {
    DebugEventType type;
    uintptr_t address;
    int pid = 0;
    int tid = 0;
    int signal = 0;
    int exitCode = 0;
    std::string moduleName;
    uintptr_t moduleBase = 0;
    size_t moduleSize = 0;
    RegisterState registers;
    std::string additionalInfo;
    std::string error;
};

// ============================================================================
// Scanner Configuration
// ============================================================================

struct ScannerConfig {
    bool enableDebugging = true;       // Attach debugger
    bool enablePatternScanning = true; // Scan for patterns
    bool enableStructAnalysis = true;  // Analyze structures
    bool autoUpdateOffsets = true;     // Write offsets to file
    bool deepScan = false;             // Extended scanning
    bool scanAllModules = true;        // Scan all loaded modules
    
    std::string targetProcess;         // Process name to target
    std::string offsetsOutputFile;     // Where to write discovered offsets
    std::string offsetsHeaderFile;     // Header file to update
    
    int scanTimeoutMs = 30000;         // Timeout for scanning operations
    int maxThreads = 4;                // Max concurrent scan threads
    
    // Known patterns for UE4 games
    std::vector<BytePattern> customPatterns;
};

/**
 * Scanner statistics
 */
struct ScannerStats {
    size_t patternsScanned = 0;
    size_t patternsFound = 0;
    size_t offsetsDiscovered = 0;
    size_t structsDiscovered = 0;
    size_t modulesScanned = 0;
    size_t bytesScanned = 0;
    double scanTimeMs = 0;
    int breakpointsHit = 0;
    int debugEvents = 0;
};

// ============================================================================
// Callback Types
// ============================================================================

using ScanCallback = std::function<void(const ScanResult&)>;
using DebugEventCallback = std::function<void(const DebugEvent&)>;
using OffsetFoundCallback = std::function<void(const DiscoveredOffset&)>;
using StructFoundCallback = std::function<void(const DiscoveredStruct&)>;
using ProgressCallback = std::function<void(int percentage, const std::string& status)>;

// ============================================================================
// Main Scanner Engine
// ============================================================================

class OffsetScannerEngine {
public:
    OffsetScannerEngine();
    ~OffsetScannerEngine();

    // ---- Initialization ----
    bool Initialize(int targetPid);
    bool Initialize(const std::string& processName);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    int GetTargetPid() const { return m_targetPid; }

    // ---- Configuration ----
    void SetConfig(const ScannerConfig& config) { m_config = config; }
    const ScannerConfig& GetConfig() const { return m_config; }
    void AddPattern(const BytePattern& pattern);
    void AddPatterns(const std::vector<BytePattern>& patterns);
    void ClearPatterns() { m_patterns.clear(); }

    // ---- Pattern Scanning ----
    ScanResult ScanPattern(const BytePattern& pattern);
    std::vector<ScanResult> ScanAllPatterns();
    std::vector<uintptr_t> FindPattern(const std::string& pattern, const std::string& moduleName = "");
    
    // ---- Module Operations ----
    struct ModuleInfo {
        std::string name;
        std::string path;
        uintptr_t base;
        size_t size;
        bool isMainModule;
    };
    std::vector<ModuleInfo> GetLoadedModules();
    ModuleInfo GetModuleInfo(const std::string& moduleName);
    ModuleInfo GetMainModule();

    // ---- Debugger Operations ----
    bool AttachDebugger();
    void DetachDebugger();
    bool IsDebuggerAttached() const { return m_debuggerAttached; }
    
    bool AddBreakpoint(uintptr_t address, BreakpointType type, const std::string& description = "");
    bool RemoveBreakpoint(uintptr_t address);
    void ClearAllBreakpoints();
    std::vector<Breakpoint> GetBreakpoints() const;
    
    bool ContinueExecution(bool singleStep = false);
    bool WaitForDebugEvent(DebugEvent& event, int timeoutMs = 1000);
    bool HandleDebugEvent(const DebugEvent& event);
    
    bool ReadRegisters(RegisterState& regs);
    bool WriteRegisters(const RegisterState& regs);
    uintptr_t GetInstructionPointer();
    uintptr_t GetStackPointer();

    // ---- Memory Analysis ----
    bool ReadMemory(uintptr_t address, void* buffer, size_t size);
    bool WriteMemory(uintptr_t address, const void* buffer, size_t size);
    std::vector<uintptr_t> FindReferences(uintptr_t address);
    std::vector<uintptr_t> FindStrings(const std::string& str);
    std::vector<uintptr_t> FindCallsTo(uintptr_t address);

    // ---- Offset Discovery ----
    DiscoveredOffset DiscoverOffset(const std::string& name, const BytePattern& pattern);
    std::vector<DiscoveredOffset> DiscoverUE4Offsets();
    std::vector<DiscoveredOffset> DiscoverActorOffsets();
    std::vector<DiscoveredOffset> DiscoverPlayerOffsets();
    std::vector<DiscoveredOffset> DiscoverWeaponOffsets();

    // ---- Structure Analysis ----
    DiscoveredStruct AnalyzeStruct(uintptr_t address, size_t maxSize = 0x1000);
    std::vector<DiscoveredStruct> DiscoverUE4Structs();
    bool CompareStructs(uintptr_t addr1, uintptr_t addr2, size_t size);

    // ---- Auto-Update ----
    bool UpdateOffsetsFile(const std::string& headerPath);
    bool GenerateOffsetsHeader(const std::string& outputPath);
    void SetOffsets(const std::map<std::string, uintptr_t>& offsets);
    std::map<std::string, uintptr_t> GetOffsets() const { return m_discoveredOffsets; }

    // ---- Scanning Operations ----
    bool RunFullScan();
    void StopScan();
    bool IsScanning() const { return m_scanning; }
    float GetScanProgress() const;

    // ---- Callbacks ----
    void SetScanCallback(ScanCallback cb) { m_scanCallback = cb; }
    void SetDebugEventCallback(DebugEventCallback cb) { m_debugEventCallback = cb; }
    void SetOffsetFoundCallback(OffsetFoundCallback cb) { m_offsetFoundCallback = cb; }
    void SetStructFoundCallback(StructFoundCallback cb) { m_structFoundCallback = cb; }
    void SetProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }

    // ---- Statistics ----
    const ScannerStats& GetStats() const { return m_stats; }
    void ResetStats();

    // ---- Utility ----
    static std::string FormatPattern(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> ParsePattern(const std::string& pattern);
    static bool MatchPattern(const uint8_t* data, const BytePattern& pattern);

private:
    // Internal implementation
    bool ScanModule(const ModuleInfo& module, const BytePattern& pattern, ScanResult& result);
    bool ScanRegion(uintptr_t start, size_t size, const BytePattern& pattern, ScanResult& result);
    
    // Debugger helpers
    bool SetupSoftwareBreakpoint(Breakpoint& bp);
    bool SetupHardwareBreakpoint(Breakpoint& bp);
    bool RemoveSoftwareBreakpoint(Breakpoint& bp);
    bool RemoveHardwareBreakpoint(Breakpoint& bp);
    
    // Pattern scanning helpers
    std::vector<uintptr_t> FindPatternInBuffer(const uint8_t* buffer, size_t size, const BytePattern& pattern);
    
    // UE4-specific analysis
    bool AnalyzeUObject(DiscoveredStruct& result);
    bool AnalyzeUClass(DiscoveredStruct& result);
    bool AnalyzeAActor(DiscoveredStruct& result);
    bool AnalyzeUWorld(DiscoveredStruct& result);
    
    // Threading
    void ScanThread();
    void DebugThread();

private:
    bool m_initialized = false;
    bool m_debuggerAttached = false;
    bool m_scanning = false;
    std::atomic<bool> m_stopScan{false};
    
    int m_targetPid = 0;
    ScannerConfig m_config;
    ScannerStats m_stats;
    
    std::vector<BytePattern> m_patterns;
    std::vector<Breakpoint> m_breakpoints;
    std::map<std::string, uintptr_t> m_discoveredOffsets;
    std::map<std::string, DiscoveredStruct> m_discoveredStructs;
    
    std::thread m_scanThread;
    std::thread m_debugThread;
    std::mutex m_mutex;
    
    // Callbacks
    ScanCallback m_scanCallback;
    DebugEventCallback m_debugEventCallback;
    OffsetFoundCallback m_offsetFoundCallback;
    StructFoundCallback m_structFoundCallback;
    ProgressCallback m_progressCallback;
};

// ============================================================================
// UE4-Specific Pattern Database
// ============================================================================

namespace UE4Patterns {
    // GWorld patterns
    std::vector<BytePattern> GetGWorldPatterns();
    
    // GNames patterns
    std::vector<BytePattern> GetGNamesPatterns();
    
    // UObject patterns
    std::vector<BytePattern> GetUObjectPatterns();
    
    // AActor patterns
    std::vector<BytePattern> GetAActorPatterns();
    
    // PlayerController patterns
    std::vector<BytePattern> GetPlayerControllerPatterns();
    
    // Weapon patterns
    std::vector<BytePattern> GetWeaponPatterns();
    
    // All known patterns
    std::vector<BytePattern> GetAllKnownPatterns();
    
    // Pattern helpers
    BytePattern CreatePattern(const std::string& pattern, const std::string& name, int32_t offset = 0);
}

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Quick scan for common UE4 offsets
 */
std::map<std::string, uintptr_t> QuickScanUE4Offsets(int pid);

/**
 * Attach and scan in one call
 */
bool AttachAndScan(const std::string& processName, ScannerConfig config = {});

/**
 * Generate offsets header from discovered offsets
 */
std::string GenerateHeaderContent(const std::map<std::string, uintptr_t>& offsets);

} // namespace OffsetScanner