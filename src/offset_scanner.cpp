#include "offset_scanner.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>

namespace OffsetScanner {

// ============================================================================
// Constructor / Destructor
// ============================================================================

OffsetScannerEngine::OffsetScannerEngine() {
    ResetStats();
}

OffsetScannerEngine::~OffsetScannerEngine() {
    Shutdown();
}

// ============================================================================
// Initialization
// ============================================================================

bool OffsetScannerEngine::Initialize(int targetPid) {
    if (m_initialized) {
        Shutdown();
    }
    
    m_targetPid = targetPid;
    
    // Verify process exists and is accessible
    std::string procPath = "/proc/" + std::to_string(targetPid);
    if (access(procPath.c_str(), F_OK) != 0) {
        return false;
    }
    
    // Check if we can read process memory
    std::string memPath = procPath + "/mem";
    if (access(memPath.c_str(), R_OK) != 0) {
        // We may need elevated privileges
    }
    
    m_initialized = true;
    return true;
}

bool OffsetScannerEngine::Initialize(const std::string& processName) {
    // Find process by name
    DIR* procDir = opendir("/proc");
    if (!procDir) return false;
    
    int foundPid = 0;
    struct dirent* entry;
    
    while ((entry = readdir(procDir)) != nullptr) {
        // Check if it's a numeric directory (process)
        if (entry->d_type != DT_DIR) continue;
        
        bool isNumeric = true;
        for (int i = 0; entry->d_name[i]; i++) {
            if (!isdigit(entry->d_name[i])) {
                isNumeric = false;
                break;
            }
        }
        
        if (!isNumeric) continue;
        
        int pid = atoi(entry->d_name);
        
        // Read process name
        std::string commPath = "/proc/" + std::string(entry->d_name) + "/comm";
        std::ifstream commFile(commPath);
        std::string commName;
        if (commFile >> commName) {
            if (commName.find(processName) != std::string::npos) {
                foundPid = pid;
                break;
            }
        }
        
        // Also check cmdline for full process path
        std::string cmdlinePath = "/proc/" + std::string(entry->d_name) + "/cmdline";
        std::ifstream cmdlineFile(cmdlinePath);
        std::string cmdline;
        if (std::getline(cmdlineFile, cmdline, '\0')) {
            if (cmdline.find(processName) != std::string::npos) {
                foundPid = pid;
                break;
            }
        }
    }
    
    closedir(procDir);
    
    if (foundPid == 0) return false;
    
    return Initialize(foundPid);
}

void OffsetScannerEngine::Shutdown() {
    StopScan();
    
    if (m_debuggerAttached) {
        DetachDebugger();
    }
    
    m_initialized = false;
    m_targetPid = 0;
    m_patterns.clear();
    m_breakpoints.clear();
    m_discoveredOffsets.clear();
    m_discoveredStructs.clear();
}

// ============================================================================
// Pattern Management
// ============================================================================

void OffsetScannerEngine::AddPattern(const BytePattern& pattern) {
    m_patterns.push_back(pattern);
}

void OffsetScannerEngine::AddPatterns(const std::vector<BytePattern>& patterns) {
    for (const auto& p : patterns) {
        m_patterns.push_back(p);
    }
}

// ============================================================================
// Pattern Parsing and Formatting
// ============================================================================

std::vector<uint8_t> OffsetScannerEngine::ParsePattern(const std::string& pattern) {
    std::vector<uint8_t> bytes;
    std::string cleaned = pattern;
    
    // Remove spaces and convert to uppercase
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());
    std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::toupper);
    
    // Parse hex bytes
    for (size_t i = 0; i < cleaned.length(); i += 2) {
        std::string byteStr = cleaned.substr(i, 2);
        if (byteStr == "??") {
            bytes.push_back(0x00);  // Wildcard - actual value doesn't matter
        } else {
            bytes.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
        }
    }
    
    return bytes;
}

std::string OffsetScannerEngine::FormatPattern(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); i++) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(bytes[i]);
        if (i < bytes.size() - 1) ss << " ";
    }
    return ss.str();
}

bool OffsetScannerEngine::MatchPattern(const uint8_t* data, const BytePattern& pattern) {
    for (size_t i = 0; i < pattern.bytes.size(); i++) {
        if (pattern.mask[i] && data[i] != pattern.bytes[i]) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Module Operations
// ============================================================================

std::vector<OffsetScannerEngine::ModuleInfo> OffsetScannerEngine::GetLoadedModules() {
    std::vector<ModuleInfo> modules;
    
    if (!m_initialized) return modules;
    
    // Read /proc/[pid]/maps
    std::string mapsPath = "/proc/" + std::to_string(m_targetPid) + "/maps";
    std::ifstream mapsFile(mapsPath);
    
    if (!mapsFile.is_open()) return modules;
    
    std::map<std::string, ModuleInfo> moduleMap;
    std::string line;
    
    while (std::getline(mapsFile, line)) {
        // Parse maps line: address perms offset dev inode pathname
        uintptr_t start, end, offset;
        char perms[5];
        unsigned int devMajor, devMinor;
        unsigned long inode;
        char pathname[512] = {0};
        
        int parsed = sscanf(line.c_str(), "%lx-%lx %4s %lx %x:%x %lu %511s",
                           &start, &end, perms, &offset, &devMajor, &devMinor, &inode, pathname);
        
        if (parsed < 7 || strlen(pathname) == 0) continue;
        
        std::string path(pathname);
        
        // Skip entries we've already processed
        if (moduleMap.find(path) != moduleMap.end()) continue;
        
        // Check if it's an executable (has 'x' permission)
        if (strchr(perms, 'x') == nullptr) continue;
        
        ModuleInfo info;
        info.path = path;
        info.base = start;
        info.size = end - start;
        
        // Extract module name from path
        size_t lastSlash = path.find_last_of('/');
        info.name = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
        
        // Check if this is the main module (first executable with valid name)
        info.isMainModule = modules.empty() && !info.name.empty();
        
        modules.push_back(info);
        moduleMap[path] = info;
    }
    
    m_stats.modulesScanned = modules.size();
    return modules;
}

OffsetScannerEngine::ModuleInfo OffsetScannerEngine::GetModuleInfo(const std::string& moduleName) {
    auto modules = GetLoadedModules();
    for (const auto& mod : modules) {
        if (mod.name == moduleName || mod.path.find(moduleName) != std::string::npos) {
            return mod;
        }
    }
    return ModuleInfo{};
}

OffsetScannerEngine::ModuleInfo OffsetScannerEngine::GetMainModule() {
    auto modules = GetLoadedModules();
    for (const auto& mod : modules) {
        if (mod.isMainModule) return mod;
    }
    if (!modules.empty()) return modules[0];
    return ModuleInfo{};
}

// ============================================================================
// Pattern Scanning
// ============================================================================

std::vector<uintptr_t> OffsetScannerEngine::FindPatternInBuffer(
    const uint8_t* buffer, size_t size, const BytePattern& pattern) {
    
    std::vector<uintptr_t> matches;
    
    if (pattern.bytes.empty() || size < pattern.bytes.size()) {
        return matches;
    }
    
    size_t scanSize = size - pattern.bytes.size();
    
    for (size_t i = 0; i <= scanSize; i++) {
        if (MatchPattern(buffer + i, pattern)) {
            matches.push_back(i);
        }
    }
    
    return matches;
}

bool OffsetScannerEngine::ScanRegion(uintptr_t start, size_t size, 
    const BytePattern& pattern, ScanResult& result) {
    
    std::vector<uint8_t> buffer(size);
    
    if (!ReadMemory(start, buffer.data(), size)) {
        result.error = "Failed to read memory region";
        return false;
    }
    
    auto matches = FindPatternInBuffer(buffer.data(), size, pattern);
    
    if (!matches.empty()) {
        result.found = true;
        result.address = start + matches[0];
        result.offset = result.address + pattern.offset;
        result.matchCount = matches.size();
        m_stats.bytesScanned += size;
        return true;
    }
    
    m_stats.bytesScanned += size;
    return false;
}

bool OffsetScannerEngine::ScanModule(const ModuleInfo& module, 
    const BytePattern& pattern, ScanResult& result) {
    
    result.name = pattern.name;
    result.moduleName = module.name;
    
    return ScanRegion(module.base, module.size, pattern, result);
}

ScanResult OffsetScannerEngine::ScanPattern(const BytePattern& pattern) {
    ScanResult result;
    result.name = pattern.name;
    
    if (!m_initialized) {
        result.error = "Scanner not initialized";
        return result;
    }
    
    std::vector<ModuleInfo> modules;
    
    if (pattern.moduleName.empty()) {
        modules = {GetMainModule()};
    } else if (pattern.moduleName == "*" || m_config.scanAllModules) {
        modules = GetLoadedModules();
    } else {
        modules = {GetModuleInfo(pattern.moduleName)};
    }
    
    for (const auto& module : modules) {
        if (module.base == 0) continue;
        
        if (ScanModule(module, pattern, result)) {
            m_stats.patternsFound++;
            return result;
        }
    }
    
    m_stats.patternsScanned++;
    result.error = "Pattern not found";
    return result;
}

std::vector<ScanResult> OffsetScannerEngine::ScanAllPatterns() {
    std::vector<ScanResult> results;
    
    for (const auto& pattern : m_patterns) {
        auto result = ScanPattern(pattern);
        results.push_back(result);
        
        if (result.found) {
            m_discoveredOffsets[pattern.name] = result.offset;
            
            if (m_scanCallback) {
                m_scanCallback(result);
            }
        }
    }
    
    return results;
}

std::vector<uintptr_t> OffsetScannerEngine::FindPattern(
    const std::string& pattern, const std::string& moduleName) {
    
    std::vector<uintptr_t> results;
    
    BytePattern bp;
    bp.bytes = ParsePattern(pattern);
    bp.patternStr = pattern;
    bp.moduleName = moduleName;
    
    // Build mask - '??' = wildcard
    std::string cleaned = pattern;
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());
    
    for (size_t i = 0; i < cleaned.length(); i += 2) {
        std::string byteStr = cleaned.substr(i, 2);
        bp.mask.push_back(byteStr != "??");
    }
    
    auto modules = moduleName.empty() ? 
        std::vector<ModuleInfo>{GetMainModule()} : 
        std::vector<ModuleInfo>{GetModuleInfo(moduleName)};
    
    for (const auto& module : modules) {
        std::vector<uint8_t> buffer(module.size);
        if (ReadMemory(module.base, buffer.data(), module.size)) {
            auto matches = FindPatternInBuffer(buffer.data(), module.size, bp);
            for (auto offset : matches) {
                results.push_back(module.base + offset);
            }
        }
    }
    
    return results;
}

// ============================================================================
// Memory Operations
// ============================================================================

bool OffsetScannerEngine::ReadMemory(uintptr_t address, void* buffer, size_t size) {
    if (!m_initialized) return false;
    
    // Try process_vm_readv first (faster, no attach needed)
    struct iovec local = { buffer, size };
    struct iovec remote = { reinterpret_cast<void*>(address), size };
    
    ssize_t bytesRead = process_vm_readv(m_targetPid, &local, 1, &remote, 1, 0);
    
    if (bytesRead == static_cast<ssize_t>(size)) {
        return true;
    }
    
    // Fall back to /proc/[pid]/mem
    std::string memPath = "/proc/" + std::to_string(m_targetPid) + "/mem";
    int memFd = open(memPath.c_str(), O_RDONLY);
    
    if (memFd == -1) return false;
    
    if (lseek(memFd, address, SEEK_SET) == static_cast<off_t>(address)) {
        ssize_t memBytesRead = ::read(memFd, buffer, size);
        close(memFd);
        return memBytesRead == static_cast<ssize_t>(size);
    }
    
    close(memFd);
    return false;
}

bool OffsetScannerEngine::WriteMemory(uintptr_t address, const void* buffer, size_t size) {
    if (!m_initialized || !m_debuggerAttached) return false;
    
    // Requires ptrace attach
    struct iovec local = { const_cast<void*>(buffer), size };
    struct iovec remote = { reinterpret_cast<void*>(address), size };
    
    ssize_t written = process_vm_writev(m_targetPid, &local, 1, &remote, 1, 0);
    
    return written == static_cast<ssize_t>(size);
}

// ============================================================================
// Debugger Operations
// ============================================================================

bool OffsetScannerEngine::AttachDebugger() {
    if (m_debuggerAttached) return true;
    if (!m_initialized) return false;
    
    // Use ptrace to attach
    if (ptrace(PTRACE_ATTACH, m_targetPid, nullptr, nullptr) == -1) {
        return false;
    }
    
    // Wait for the process to stop
    int status;
    if (waitpid(m_targetPid, &status, 0) == -1) {
        ptrace(PTRACE_DETACH, m_targetPid, nullptr, nullptr);
        return false;
    }
    
    if (!WIFSTOPPED(status)) {
        ptrace(PTRACE_DETACH, m_targetPid, nullptr, nullptr);
        return false;
    }
    
    m_debuggerAttached = true;
    return true;
}

void OffsetScannerEngine::DetachDebugger() {
    if (!m_debuggerAttached) return;
    
    // Remove all breakpoints first
    ClearAllBreakpoints();
    
    // Detach
    ptrace(PTRACE_DETACH, m_targetPid, nullptr, nullptr);
    m_debuggerAttached = false;
}

bool OffsetScannerEngine::AddBreakpoint(uintptr_t address, BreakpointType type, 
    const std::string& description) {
    
    if (!m_debuggerAttached) return false;
    
    // Check if breakpoint already exists
    for (const auto& bp : m_breakpoints) {
        if (bp.address == address) return false;
    }
    
    Breakpoint bp;
    bp.address = address;
    bp.type = type;
    bp.description = description;
    
    if (type == BreakpointType::EXECUTE) {
        if (!SetupSoftwareBreakpoint(bp)) return false;
    } else {
        if (!SetupHardwareBreakpoint(bp)) return false;
    }
    
    m_breakpoints.push_back(bp);
    return true;
}

bool OffsetScannerEngine::SetupSoftwareBreakpoint(Breakpoint& bp) {
    // Read original byte
    errno = 0;
    long data = ptrace(PTRACE_PEEKTEXT, m_targetPid, reinterpret_cast<void*>(bp.address), nullptr);
    
    if (errno != 0) return false;
    
    bp.originalByte = static_cast<uint8_t>(data & 0xFF);
    
    // Write INT3 (0xCC)
    long int3 = (data & ~0xFF) | 0xCC;
    
    if (ptrace(PTRACE_POKETEXT, m_targetPid, reinterpret_cast<void*>(bp.address), 
               reinterpret_cast<void*>(int3)) == -1) {
        return false;
    }
    
    bp.enabled = true;
    return true;
}

bool OffsetScannerEngine::SetupHardwareBreakpoint(Breakpoint& bp) {
    // Find available debug register
    int drIndex = -1;
    for (int i = 0; i < 4; i++) {
        bool inUse = false;
        for (const auto& existing : m_breakpoints) {
            if (existing.drIndex == i) {
                inUse = true;
                break;
            }
        }
        if (!inUse) {
            drIndex = i;
            break;
        }
    }
    
    if (drIndex == -1) return false;  // No available debug register
    
    bp.drIndex = drIndex;
    
    // Calculate offset for debug register (each is 8 bytes)
    size_t drOffset = offsetof(struct user, u_debugreg) + drIndex * sizeof(uintptr_t);
    size_t dr7Offset = offsetof(struct user, u_debugreg) + 7 * sizeof(uintptr_t);
    
    // Read current debug register state
    errno = 0;
    long dr7 = ptrace(PTRACE_PEEKUSER, m_targetPid, 
                      reinterpret_cast<void*>(dr7Offset), nullptr);
    
    if (errno != 0) return false;
    
    // Set the address
    if (ptrace(PTRACE_POKEUSER, m_targetPid, 
               reinterpret_cast<void*>(drOffset),
               reinterpret_cast<void*>(bp.address)) == -1) {
        return false;
    }
    
    // Configure DR7
    int typeBits = 0;
    switch (bp.type) {
        case BreakpointType::WRITE: typeBits = 0x01; break;
        case BreakpointType::ACCESS: typeBits = 0x03; break;
        case BreakpointType::READ:
        case BreakpointType::EXECUTE: typeBits = 0x00; break;
    }
    
    // Set enable bit and type/length bits
    dr7 |= (1 << (drIndex * 2));  // Local enable
    dr7 |= (typeBits << (16 + drIndex * 4));  // Type
    dr7 |= (0x00 << (18 + drIndex * 4));  // Length (1 byte)
    
    if (ptrace(PTRACE_POKEUSER, m_targetPid, 
               reinterpret_cast<void*>(dr7Offset),
               reinterpret_cast<void*>(dr7)) == -1) {
        return false;
    }
    
    bp.enabled = true;
    return true;
}

bool OffsetScannerEngine::RemoveBreakpoint(uintptr_t address) {
    for (auto it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it) {
        if (it->address == address) {
            if (it->type == BreakpointType::EXECUTE) {
                RemoveSoftwareBreakpoint(*it);
            } else {
                RemoveHardwareBreakpoint(*it);
            }
            m_breakpoints.erase(it);
            return true;
        }
    }
    return false;
}

bool OffsetScannerEngine::RemoveSoftwareBreakpoint(Breakpoint& bp) {
    if (!bp.enabled) return true;
    
    // Restore original byte
    errno = 0;
    long data = ptrace(PTRACE_PEEKTEXT, m_targetPid, 
                       reinterpret_cast<void*>(bp.address), nullptr);
    
    if (errno != 0) return false;
    
    long restored = (data & ~0xFF) | bp.originalByte;
    
    if (ptrace(PTRACE_POKETEXT, m_targetPid, 
               reinterpret_cast<void*>(bp.address),
               reinterpret_cast<void*>(restored)) == -1) {
        return false;
    }
    
    bp.enabled = false;
    return true;
}

bool OffsetScannerEngine::RemoveHardwareBreakpoint(Breakpoint& bp) {
    if (!bp.enabled) return true;
    
    // Calculate offsets
    size_t drOffset = offsetof(struct user, u_debugreg) + bp.drIndex * sizeof(uintptr_t);
    size_t dr7Offset = offsetof(struct user, u_debugreg) + 7 * sizeof(uintptr_t);
    
    // Clear the address
    ptrace(PTRACE_POKEUSER, m_targetPid, 
           reinterpret_cast<void*>(drOffset),
           nullptr);
    
    // Clear enable bit in DR7
    errno = 0;
    long dr7 = ptrace(PTRACE_PEEKUSER, m_targetPid, 
                      reinterpret_cast<void*>(dr7Offset), nullptr);
    
    if (errno == 0) {
        dr7 &= ~(1 << (bp.drIndex * 2));
        dr7 &= ~(0x0F << (16 + bp.drIndex * 4));
        
        ptrace(PTRACE_POKEUSER, m_targetPid, 
               reinterpret_cast<void*>(dr7Offset),
               reinterpret_cast<void*>(dr7));
    }
    
    bp.enabled = false;
    bp.drIndex = -1;
    return true;
}

void OffsetScannerEngine::ClearAllBreakpoints() {
    for (auto& bp : m_breakpoints) {
        if (bp.type == BreakpointType::EXECUTE) {
            RemoveSoftwareBreakpoint(bp);
        } else {
            RemoveHardwareBreakpoint(bp);
        }
    }
    m_breakpoints.clear();
}

std::vector<Breakpoint> OffsetScannerEngine::GetBreakpoints() const {
    return m_breakpoints;
}

bool OffsetScannerEngine::ContinueExecution(bool singleStep) {
    if (!m_debuggerAttached) return false;
    
    __ptrace_request request = singleStep ? PTRACE_SINGLESTEP : PTRACE_CONT;
    return ptrace(request, m_targetPid, nullptr, nullptr) == 0;
}

bool OffsetScannerEngine::WaitForDebugEvent(DebugEvent& event, int timeoutMs) {
    if (!m_debuggerAttached) return false;
    
    int status;
    int options = __WALL;
    
    // Use waitpid with timeout via sigaction
    struct sigaction sa, oldSa;
    sa.sa_handler = [](int) {};
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    
    sigaction(SIGALRM, &sa, &oldSa);
    ualarm(timeoutMs * 1000, 0);
    
    pid_t result = waitpid(m_targetPid, &status, 0);
    
    ualarm(0, 0);
    sigaction(SIGALRM, &oldSa, nullptr);
    
    if (result == -1) {
        event.type = DebugEventType::EXCEPTION;
        event.error = "waitpid failed or timeout";
        return false;
    }
    
    m_stats.debugEvents++;
    
    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        event.signal = sig;
        event.pid = m_targetPid;
        
        switch (sig) {
            case SIGTRAP: {
                // Check if it's a breakpoint
                event.type = DebugEventType::BREAKPOINT;
                
                // Get current RIP
                RegisterState regs;
                if (ReadRegisters(regs)) {
                    event.registers = regs;
                    event.address = regs.rip;
                    
                    // Find matching breakpoint
                    for (auto& bp : m_breakpoints) {
                        if (bp.address == regs.rip - 1) {  // RIP is after INT3
                            bp.hitCount++;
                            event.address = bp.address;
                            event.additionalInfo = bp.description;
                            break;
                        }
                    }
                }
                
                m_stats.breakpointsHit++;
                break;
            }
            
            case SIGSEGV:
            case SIGFPE:
            case SIGBUS:
                event.type = DebugEventType::EXCEPTION;
                event.address = GetInstructionPointer();
                break;
            
            default:
                event.type = DebugEventType::SINGLE_STEP;
                break;
        }
        
        if (m_debugEventCallback) {
            m_debugEventCallback(event);
        }
        
        return true;
    }
    
    if (WIFEXITED(status)) {
        event.type = DebugEventType::PROCESS_EXIT;
        event.exitCode = WEXITSTATUS(status);
        return true;
    }
    
    return false;
}

bool OffsetScannerEngine::ReadRegisters(RegisterState& regs) {
    if (!m_debuggerAttached) return false;
    
    struct user_regs_struct ptraceRegs;
    
    if (ptrace(PTRACE_GETREGS, m_targetPid, nullptr, &ptraceRegs) == -1) {
        return false;
    }
    
    regs.rax = ptraceRegs.rax;
    regs.rbx = ptraceRegs.rbx;
    regs.rcx = ptraceRegs.rcx;
    regs.rdx = ptraceRegs.rdx;
    regs.rsi = ptraceRegs.rsi;
    regs.rdi = ptraceRegs.rdi;
    regs.rbp = ptraceRegs.rbp;
    regs.rsp = ptraceRegs.rsp;
    regs.r8 = ptraceRegs.r8;
    regs.r9 = ptraceRegs.r9;
    regs.r10 = ptraceRegs.r10;
    regs.r11 = ptraceRegs.r11;
    regs.r12 = ptraceRegs.r12;
    regs.r13 = ptraceRegs.r13;
    regs.r14 = ptraceRegs.r14;
    regs.r15 = ptraceRegs.r15;
    regs.rip = ptraceRegs.rip;
    regs.rflags = ptraceRegs.eflags;
    regs.cs = ptraceRegs.cs;
    regs.ds = ptraceRegs.ds;
    regs.es = ptraceRegs.es;
    regs.fs = ptraceRegs.fs;
    regs.gs = ptraceRegs.gs;
    regs.ss = ptraceRegs.ss;
    
    return true;
}

bool OffsetScannerEngine::WriteRegisters(const RegisterState& regs) {
    if (!m_debuggerAttached) return false;
    
    struct user_regs_struct ptraceRegs = {};
    ptraceRegs.rax = regs.rax;
    ptraceRegs.rbx = regs.rbx;
    ptraceRegs.rcx = regs.rcx;
    ptraceRegs.rdx = regs.rdx;
    ptraceRegs.rsi = regs.rsi;
    ptraceRegs.rdi = regs.rdi;
    ptraceRegs.rbp = regs.rbp;
    ptraceRegs.rsp = regs.rsp;
    ptraceRegs.r8 = regs.r8;
    ptraceRegs.r9 = regs.r9;
    ptraceRegs.r10 = regs.r10;
    ptraceRegs.r11 = regs.r11;
    ptraceRegs.r12 = regs.r12;
    ptraceRegs.r13 = regs.r13;
    ptraceRegs.r14 = regs.r14;
    ptraceRegs.r15 = regs.r15;
    ptraceRegs.rip = regs.rip;
    ptraceRegs.eflags = regs.rflags;
    
    return ptrace(PTRACE_SETREGS, m_targetPid, nullptr, &ptraceRegs) == 0;
}

uintptr_t OffsetScannerEngine::GetInstructionPointer() {
    RegisterState regs;
    if (ReadRegisters(regs)) {
        return regs.rip;
    }
    return 0;
}

uintptr_t OffsetScannerEngine::GetStackPointer() {
    RegisterState regs;
    if (ReadRegisters(regs)) {
        return regs.rsp;
    }
    return 0;
}

// ============================================================================
// Offset Discovery
// ============================================================================

DiscoveredOffset OffsetScannerEngine::DiscoverOffset(
    const std::string& name, const BytePattern& pattern) {
    
    DiscoveredOffset offset;
    offset.name = name;
    offset.signature = pattern.patternStr;
    
    auto result = ScanPattern(pattern);
    
    if (result.found) {
        offset.offset = result.offset;
        offset.verified = true;
        offset.description = "Found at 0x" + std::to_string(result.address);
        
        m_discoveredOffsets[name] = result.offset;
        m_stats.offsetsDiscovered++;
        
        if (m_offsetFoundCallback) {
            m_offsetFoundCallback(offset);
        }
    }
    
    return offset;
}

std::vector<DiscoveredOffset> OffsetScannerEngine::DiscoverUE4Offsets() {
    std::vector<DiscoveredOffset> offsets;
    
    // Common UE4 patterns
    auto patterns = UE4Patterns::GetAllKnownPatterns();
    
    for (const auto& pattern : patterns) {
        auto offset = DiscoverOffset(pattern.name, pattern);
        if (offset.verified) {
            offsets.push_back(offset);
        }
    }
    
    return offsets;
}

std::vector<DiscoveredOffset> OffsetScannerEngine::DiscoverActorOffsets() {
    std::vector<DiscoveredOffset> offsets;
    
    auto patterns = UE4Patterns::GetAActorPatterns();
    
    for (const auto& pattern : patterns) {
        auto offset = DiscoverOffset(pattern.name, pattern);
        if (offset.verified) {
            offsets.push_back(offset);
        }
    }
    
    return offsets;
}

std::vector<DiscoveredOffset> OffsetScannerEngine::DiscoverPlayerOffsets() {
    std::vector<DiscoveredOffset> offsets;
    
    auto patterns = UE4Patterns::GetPlayerControllerPatterns();
    
    for (const auto& pattern : patterns) {
        auto offset = DiscoverOffset(pattern.name, pattern);
        if (offset.verified) {
            offsets.push_back(offset);
        }
    }
    
    return offsets;
}

std::vector<DiscoveredOffset> OffsetScannerEngine::DiscoverWeaponOffsets() {
    std::vector<DiscoveredOffset> offsets;
    
    auto patterns = UE4Patterns::GetWeaponPatterns();
    
    for (const auto& pattern : patterns) {
        auto offset = DiscoverOffset(pattern.name, pattern);
        if (offset.verified) {
            offsets.push_back(offset);
        }
    }
    
    return offsets;
}

// ============================================================================
// Structure Analysis
// ============================================================================

DiscoveredStruct OffsetScannerEngine::AnalyzeStruct(uintptr_t address, size_t maxSize) {
    DiscoveredStruct result;
    result.patternAddress = address;
    
    if (address == 0) return result;
    
    // Read memory at address
    std::vector<uint8_t> buffer(maxSize);
    if (!ReadMemory(address, buffer.data(), maxSize)) {
        return result;
    }
    
    // Analyze for pointers (8-byte aligned, valid memory range)
    for (size_t offset = 0; offset + 8 <= maxSize; offset += 8) {
        uintptr_t potentialPtr = *reinterpret_cast<uintptr_t*>(&buffer[offset]);
        
        // Check if it looks like a valid pointer (typically in lower or canonical range)
        if (potentialPtr > 0x10000 && potentialPtr < 0x7FFFFFFFFFFF) {
            DiscoveredOffset member;
            member.offset = offset;
            member.typeName = "void*";
            member.description = "Potential pointer";
            result.members.push_back(member);
        }
    }
    
    result.size = maxSize;
    result.isComplete = result.members.size() > 0;
    m_stats.structsDiscovered++;
    
    return result;
}

bool OffsetScannerEngine::CompareStructs(uintptr_t addr1, uintptr_t addr2, size_t size) {
    std::vector<uint8_t> buf1(size), buf2(size);
    
    if (!ReadMemory(addr1, buf1.data(), size)) return false;
    if (!ReadMemory(addr2, buf2.data(), size)) return false;
    
    return memcmp(buf1.data(), buf2.data(), size) == 0;
}

std::vector<DiscoveredStruct> OffsetScannerEngine::DiscoverUE4Structs() {
    std::vector<DiscoveredStruct> structs;
    
    // Analyze UObject structure
    DiscoveredStruct uobject;
    uobject.name = "UObject";
    if (AnalyzeUObject(uobject)) {
        structs.push_back(uobject);
        m_discoveredStructs[uobject.name] = uobject;
    }
    
    // Analyze UClass structure
    DiscoveredStruct uclass;
    uclass.name = "UClass";
    if (AnalyzeUClass(uclass)) {
        structs.push_back(uclass);
        m_discoveredStructs[uclass.name] = uclass;
    }
    
    // Analyze AActor structure
    DiscoveredStruct actor;
    actor.name = "AActor";
    if (AnalyzeAActor(actor)) {
        structs.push_back(actor);
        m_discoveredStructs[actor.name] = actor;
    }
    
    // Analyze UWorld structure
    DiscoveredStruct world;
    world.name = "UWorld";
    if (AnalyzeUWorld(world)) {
        structs.push_back(world);
        m_discoveredStructs[world.name] = world;
    }
    
    return structs;
}

bool OffsetScannerEngine::AnalyzeUObject(DiscoveredStruct& result) {
    // UE4 UObject has known structure layout
    // This would normally scan for actual instances in memory
    
    // Add known UObject members
    DiscoveredOffset vtable;
    vtable.name = "VTable";
    vtable.offset = 0x0;
    vtable.typeName = "void**";
    vtable.description = "Virtual function table pointer";
    result.members.push_back(vtable);
    
    DiscoveredOffset objectFlags;
    objectFlags.name = "ObjectFlags";
    objectFlags.offset = 0x8;
    objectFlags.typeName = "int32";
    objectFlags.description = "Object flags";
    result.members.push_back(objectFlags);
    
    DiscoveredOffset internalIndex;
    internalIndex.name = "InternalIndex";
    internalIndex.offset = 0xC;
    internalIndex.typeName = "int32";
    internalIndex.description = "Internal object index";
    result.members.push_back(internalIndex);
    
    DiscoveredOffset classPtr;
    classPtr.name = "Class";
    classPtr.offset = 0x10;
    classPtr.typeName = "UClass*";
    classPtr.description = "Pointer to class";
    result.members.push_back(classPtr);
    
    DiscoveredOffset name;
    name.name = "Name";
    name.offset = 0x18;
    name.typeName = "FName";
    name.description = "Object name";
    result.members.push_back(name);
    
    DiscoveredOffset outer;
    outer.name = "Outer";
    outer.offset = 0x20;
    outer.typeName = "UObject*";
    outer.description = "Outer object";
    result.members.push_back(outer);
    
    result.size = 0x28;
    result.isComplete = true;
    m_stats.structsDiscovered++;
    
    return true;
}

bool OffsetScannerEngine::AnalyzeUClass(DiscoveredStruct& result) {
    // UClass inherits from UObject, so start with UObject members
    AnalyzeUObject(result);
    result.name = "UClass";
    
    // Add UClass-specific members
    DiscoveredOffset super;
    super.name = "SuperClass";
    super.offset = result.size;
    super.typeName = "UClass*";
    super.description = "Super class pointer";
    result.members.push_back(super);
    
    result.size += sizeof(void*);
    result.isComplete = true;
    
    return true;
}

bool OffsetScannerEngine::AnalyzeAActor(DiscoveredStruct& result) {
    // AActor inherits from UObject
    AnalyzeUObject(result);
    result.name = "AActor";
    
    // Add AActor-specific members
    DiscoveredOffset rootComponent;
    rootComponent.name = "RootComponent";
    rootComponent.offset = result.size;
    rootComponent.typeName = "USceneComponent*";
    rootComponent.description = "Root scene component";
    result.members.push_back(rootComponent);
    
    DiscoveredOffset playerState;
    playerState.name = "PlayerState";
    playerState.offset = result.size + sizeof(void*);
    playerState.typeName = "APlayerState*";
    playerState.description = "Player state";
    result.members.push_back(playerState);
    
    result.size += sizeof(void*) * 2;
    result.isComplete = true;
    
    return true;
}

bool OffsetScannerEngine::AnalyzeUWorld(DiscoveredStruct& result) {
    // UWorld inherits from UObject
    AnalyzeUObject(result);
    result.name = "UWorld";
    
    // Add UWorld-specific members
    DiscoveredOffset gameInstance;
    gameInstance.name = "GameInstance";
    gameInstance.offset = result.size;
    gameInstance.typeName = "UGameInstance*";
    gameInstance.description = "Game instance";
    result.members.push_back(gameInstance);
    
    DiscoveredOffset levels;
    levels.name = "Levels";
    levels.offset = result.size + sizeof(void*);
    levels.typeName = "TArray<ULevel*>";
    levels.description = "Array of levels";
    result.members.push_back(levels);
    
    result.size += sizeof(void*) * 2;
    result.isComplete = true;
    
    return true;
}

// ============================================================================
// Auto-Update Functions
// ============================================================================

bool OffsetScannerEngine::UpdateOffsetsFile(const std::string& headerPath) {
    std::ifstream inFile(headerPath);
    if (!inFile.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(inFile)),
                        std::istreambuf_iterator<char>());
    inFile.close();
    
    // Update each discovered offset in the file
    for (const auto& [name, offset] : m_discoveredOffsets) {
        // Pattern to match: constexpr uintptr_t NAME = 0xHEX;
        std::regex pattern(
            "(constexpr\\s+uintptr_t\\s+" + name + "\\s*=\\s*)0x[0-9A-Fa-f]+(;)"
        );
        
        char replacement[64];
        snprintf(replacement, sizeof(replacement), "$10x%lX$2", offset);
        
        content = std::regex_replace(content, pattern, replacement);
    }
    
    std::ofstream outFile(headerPath);
    if (!outFile.is_open()) return false;
    
    outFile << content;
    return true;
}

bool OffsetScannerEngine::GenerateOffsetsHeader(const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) return false;
    
    file << "// Auto-generated offsets\n";
    file << "// Generated by OffsetScanner\n\n";
    file << "#pragma once\n\n";
    file << "#include <cstdint>\n\n";
    file << "namespace Offsets {\n";
    
    for (const auto& [name, offset] : m_discoveredOffsets) {
        file << "    constexpr uintptr_t " << name << " = 0x" 
             << std::hex << offset << ";\n";
    }
    
    file << "}\n";
    
    return true;
}

void OffsetScannerEngine::SetOffsets(const std::map<std::string, uintptr_t>& offsets) {
    m_discoveredOffsets = offsets;
}

// ============================================================================
// Scanning Operations
// ============================================================================

bool OffsetScannerEngine::RunFullScan() {
    if (m_scanning) return false;
    if (!m_initialized) return false;
    
    m_scanning = true;
    m_stopScan = false;
    ResetStats();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Attach debugger if enabled
    if (m_config.enableDebugging && !AttachDebugger()) {
        // Continue without debugger
    }
    
    // Run pattern scans
    if (m_config.enablePatternScanning) {
        auto results = ScanAllPatterns();
        
        for (const auto& result : results) {
            if (m_stopScan) break;
        }
    }
    
    // Analyze structures if enabled
    if (m_config.enableStructAnalysis && !m_stopScan) {
        DiscoverUE4Structs();
    }
    
    // Auto-update offsets file if enabled
    if (m_config.autoUpdateOffsets && !m_config.offsetsHeaderFile.empty()) {
        UpdateOffsetsFile(m_config.offsetsHeaderFile);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.scanTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    // Detach debugger if we attached
    if (m_debuggerAttached) {
        DetachDebugger();
    }
    
    m_scanning = false;
    return true;
}

void OffsetScannerEngine::StopScan() {
    m_stopScan = true;
    
    if (m_scanThread.joinable()) {
        m_scanThread.join();
    }
}

float OffsetScannerEngine::GetScanProgress() const {
    if (m_patterns.empty()) return 0.0f;
    return static_cast<float>(m_stats.patternsScanned) / m_patterns.size() * 100.0f;
}

void OffsetScannerEngine::ResetStats() {
    m_stats = ScannerStats{};
}

// ============================================================================
// UE4 Pattern Database
// ============================================================================

namespace UE4Patterns {

BytePattern CreatePattern(const std::string& pattern, const std::string& name, int32_t offset) {
    BytePattern bp;
    bp.patternStr = pattern;
    bp.name = name;
    bp.offset = offset;
    
    // Parse pattern string
    std::string cleaned = pattern;
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());
    
    for (size_t i = 0; i < cleaned.length(); i += 2) {
        std::string byteStr = cleaned.substr(i, 2);
        if (byteStr == "??") {
            bp.bytes.push_back(0);
            bp.mask.push_back(false);
        } else {
            bp.bytes.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
            bp.mask.push_back(true);
        }
    }
    
    return bp;
}

std::vector<BytePattern> GetGWorldPatterns() {
    return {
        // Linux / Proton specific UE4.2x - UE5.x patterns (RIP-relative)
        CreatePattern("48 8B 05 ?? ?? ?? ?? 48 8B 88 ?? ?? ?? ?? 48 85 C9", "GWorld", 3), // +3 for RIP skip
        CreatePattern("48 8B 1D ?? ?? ?? ?? 48 85 DB 74 ?? 48 8B 05", "GWorld_Alt", 3),
        CreatePattern("48 83 EC 28 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 88", "GWorld_Call", 7),
    };
}

std::vector<BytePattern> GetGNamesPatterns() {
    return {
        // Linux / Proton specialized FNamePool patterns
        CreatePattern("48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 1D ?? ?? ?? ?? 48 85 DB", "GNames", 3),
        CreatePattern("48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 1D", "GNames_Alt", 3),
        CreatePattern("48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 48 89 05", "GNames_Set", 3),
    };
}

std::vector<BytePattern> GetUObjectPatterns() {
    return {
        // FUObjectArray patterns (Internal UE4 structure)
        CreatePattern("48 8B 05 ?? ?? ?? ?? 48 8B 0C C8 48 85 C9 74 ?? 48 8B 01", "GUObjectArray", 3),
        CreatePattern("48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 0D", "GUObjectArray_Alt", 3),
        CreatePattern("48 8B 42 08 48 8B 0C C8 48 85 C9", "UObject_ProcessEvent", 0),
    };
}

std::vector<BytePattern> GetAActorPatterns() {
    return {
        CreatePattern("48 8B 89 ?? ?? ?? ?? 48 85 C9 74", "AActor_RootComponent", 0),
        CreatePattern("48 8B 87 ?? ?? ?? ?? 48 85 C0 74", "AActor_PlayerState", 0),
        CreatePattern("0F 84 ?? ?? ?? ?? 48 8B 87 ?? ?? ?? ?? 48 8B CF", "AActor_Health", 0),
    };
}

std::vector<BytePattern> GetPlayerControllerPatterns() {
    return {
        CreatePattern("48 8B 87 ?? ?? ?? ?? 48 85 C0 74", "APlayerController_Pawn", 0),
        CreatePattern("48 8B 8F ?? ?? ?? ?? 48 85 C9 74", "APlayerController_CameraManager", 0),
        CreatePattern("48 8B 80 ?? ?? ?? ?? 48 85 C0 74", "APlayerController_ControlRotation", 0),
    };
}

std::vector<BytePattern> GetWeaponPatterns() {
    return {
        CreatePattern("48 8B 87 ?? ?? ?? ?? 48 85 C0 74", "AWeapon_Owner", 0),
        CreatePattern("0F 2F 80 ?? ?? ?? ?? 76", "AWeapon_Damage", 0),
        CreatePattern("F3 0F 10 87 ?? ?? ?? ?? F3 0F 59", "AWeapon_FireRate", 0),
    };
}

std::vector<BytePattern> GetAllKnownPatterns() {
    std::vector<BytePattern> all;
    
    auto gworld = GetGWorldPatterns();
    auto gnames = GetGNamesPatterns();
    auto uobject = GetUObjectPatterns();
    auto actor = GetAActorPatterns();
    auto player = GetPlayerControllerPatterns();
    auto weapon = GetWeaponPatterns();
    
    all.insert(all.end(), gworld.begin(), gworld.end());
    all.insert(all.end(), gnames.begin(), gnames.end());
    all.insert(all.end(), uobject.begin(), uobject.end());
    all.insert(all.end(), actor.begin(), actor.end());
    all.insert(all.end(), player.begin(), player.end());
    all.insert(all.end(), weapon.begin(), weapon.end());
    
    // ViewMatrix pattern
    all.push_back(CreatePattern("0F 10 05 ?? ?? ?? ?? 0F 11 05 ?? ?? ?? ?? 0F 10 40 ?? 0F 11 45", "ViewMatrix", 3));
    
    return all;
}

} // namespace UE4Patterns

// ============================================================================
// Convenience Functions
// ============================================================================

std::map<std::string, uintptr_t> QuickScanUE4Offsets(int pid) {
    OffsetScannerEngine scanner;
    if (!scanner.Initialize(pid)) return {};
    
    scanner.AddPatterns(UE4Patterns::GetAllKnownPatterns());
    scanner.RunFullScan();
    
    return scanner.GetOffsets();
}

bool AttachAndScan(const std::string& processName, ScannerConfig config) {
    OffsetScannerEngine scanner;
    scanner.SetConfig(config);
    
    if (!scanner.Initialize(processName)) return false;
    
    scanner.AddPatterns(UE4Patterns::GetAllKnownPatterns());
    return scanner.RunFullScan();
}

std::string GenerateHeaderContent(const std::map<std::string, uintptr_t>& offsets) {
    std::ostringstream oss;
    
    oss << "// Auto-generated UE4 offsets\n";
    oss << "#pragma once\n\n";
    oss << "#include <cstdint>\n\n";
    oss << "namespace Offsets {\n";
    
    for (const auto& [name, offset] : offsets) {
        oss << "    constexpr uintptr_t " << name << " = 0x" 
            << std::hex << offset << ";\n";
    }
    
    oss << "}\n";
    
    return oss.str();
}

} // namespace OffsetScanner