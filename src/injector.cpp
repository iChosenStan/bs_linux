#include "injector.h"

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Injector {

//=============================================================================
// Helper Functions
//=============================================================================

static std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

//=============================================================================
// Process Functions
//=============================================================================

ProcessInfo FindProcess(const std::string& processName) {
    ProcessInfo info = {};
    
    DIR* procDir = opendir("/proc");
    if (!procDir) return info;
    
    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // Check if it's a numeric directory (PID)
        if (entry->d_type != DT_DIR) continue;
        
        uint32_t pid = 0;
        try {
            pid = std::stoul(entry->d_name);
        } catch (...) {
            continue;
        }
        
        // Read /proc/[pid]/comm to get process name
        std::string commPath = "/proc/" + std::string(entry->d_name) + "/comm";
        std::string comm = ReadFile(commPath);
        
        // Remove trailing newline
        if (!comm.empty() && comm.back() == '\n') {
            comm.pop_back();
        }
        
        // Check if this is our target process
        if (comm == processName) {
            info.pid = pid;
            info.name = comm;
            
            // Get executable path
            std::string exePath = "/proc/" + std::string(entry->d_name) + "/exe";
            char exeBuf[512];
            ssize_t len = readlink(exePath.c_str(), exeBuf, sizeof(exeBuf) - 1);
            if (len > 0) {
                exeBuf[len] = '\0';
                info.path = exeBuf;
            }
            
            break;
        }
    }
    
    closedir(procDir);
    return info;
}

uintptr_t GetModuleBase(uint32_t pid, const std::string& moduleName) {
    std::string mapsPath = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream mapsFile(mapsPath);
    
    if (!mapsFile.is_open()) return 0;
    
    std::string line;
    while (std::getline(mapsFile, line)) {
        if (line.find(moduleName) != std::string::npos) {
            // Parse address range
            size_t dashPos = line.find('-');
            if (dashPos != std::string::npos) {
                std::string addrStr = line.substr(0, dashPos);
                return std::stoull(addrStr, nullptr, 16);
            }
        }
    }
    
    return 0;
}

//=============================================================================
// Ptrace Functions
//=============================================================================

bool Attach(uint32_t pid) {
    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) == -1) {
        perror("ptrace attach");
        return false;
    }
    
    // Wait for process to stop
    int status;
    waitpid(pid, &status, 0);
    return true;
}

void Detach(uint32_t pid) {
    ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
}

std::vector<uint32_t> GetThreads(uint32_t pid) {
    std::vector<uint32_t> threads;
    
    std::string taskPath = "/proc/" + std::to_string(pid) + "/task";
    DIR* taskDir = opendir(taskPath.c_str());
    
    if (!taskDir) return threads;
    
    struct dirent* entry;
    while ((entry = readdir(taskDir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            uint32_t tid = 0;
            try {
                tid = std::stoul(entry->d_name);
                threads.push_back(tid);
            } catch (...) {
                continue;
            }
        }
    }
    
    closedir(taskDir);
    return threads;
}

//=============================================================================
// Memory Functions
//=============================================================================

bool ReadMemory(uint32_t pid, uintptr_t address, void* buffer, size_t size) {
    struct iovec local = {
        .iov_base = buffer,
        .iov_len = size
    };
    
    struct iovec remote = {
        .iov_base = (void*)address,
        .iov_len = size
    };
    
    ssize_t result = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    return result == static_cast<ssize_t>(size);
}

bool WriteMemory(uint32_t pid, uintptr_t address, const void* buffer, size_t size) {
    struct iovec local = {
        .iov_base = (void*)buffer,  // Note: const cast needed for API
        .iov_len = size
    };
    
    struct iovec remote = {
        .iov_base = (void*)address,
        .iov_len = size
    };
    
    ssize_t result = process_vm_writev(pid, &local, 1, &remote, 1, 0);
    return result == static_cast<ssize_t>(size);
}

uintptr_t AllocateMemory(uint32_t pid, size_t size, int protection) {
    // Use ptrace to inject mmap syscall
    // This is a simplified implementation
    
    // Get threads
    auto threads = GetThreads(pid);
    if (threads.empty()) return 0;
    
    uint32_t tid = threads[0];
    
    // Attach to thread
    if (!Attach(tid)) return 0;
    
    // Get current registers
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, tid, nullptr, &regs) == -1) {
        Detach(tid);
        return 0;
    }
    
    // Save original registers
    struct user_regs_struct origRegs = regs;
    
    // Prepare mmap syscall
    // mmap(NULL, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    regs.rax = SYS_mmap;           // syscall number
    regs.rdi = 0;                  // addr = NULL
    regs.rsi = size;               // length
    regs.rdx = protection;         // prot
    regs.r10 = MAP_PRIVATE | MAP_ANONYMOUS;  // flags
    regs.r8 = 0xFFFFFFFF;          // fd = -1
    regs.r9 = 0;                   // offset
    
    // Set registers
    if (ptrace(PTRACE_SETREGS, tid, nullptr, &regs) == -1) {
        Detach(tid);
        return 0;
    }
    
    // Execute syscall
    // We need to write a syscall instruction somewhere
    // For simplicity, use a known executable region
    
    // Find executable region
    uintptr_t codeAddr = GetModuleBase(pid, "") + 0x1000;  // Approximate
    
    // Backup original bytes
    long origWord = ptrace(PTRACE_PEEKTEXT, tid, (void*)codeAddr, nullptr);
    
    // Write syscall instruction (0x0F 0x05)
    ptrace(PTRACE_POKETEXT, tid, (void*)codeAddr, (void*)0x050F);
    
    // Set RIP to syscall
    regs.rip = codeAddr;
    ptrace(PTRACE_SETREGS, tid, nullptr, &regs);
    
    // Single step
    ptrace(PTRACE_SINGLESTEP, tid, nullptr, nullptr);
    
    int status;
    waitpid(tid, &status, 0);
    
    // Get result
    ptrace(PTRACE_GETREGS, tid, nullptr, &regs);
    uintptr_t result = regs.rax;
    
    // Restore original bytes
    ptrace(PTRACE_POKETEXT, tid, (void*)codeAddr, (void*)origWord);
    
    // Restore original registers
    ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
    
    // Detach
    Detach(tid);
    
    return result;
}

//=============================================================================
// Library Injection
//=============================================================================

bool InjectLibrary(uint32_t pid, const std::string& libraryPath) {
    printf("[*] Injecting %s into process %u\n", libraryPath.c_str(), pid);
    
    // Get threads
    auto threads = GetThreads(pid);
    if (threads.empty()) {
        fprintf(stderr, "[-] Failed to get threads\n");
        return false;
    }
    
    uint32_t tid = threads[0];
    
    // Attach to thread
    if (!Attach(tid)) {
        fprintf(stderr, "[-] Failed to attach to thread %u\n", tid);
        return false;
    }
    
    printf("[+] Attached to thread %u\n", tid);
    
    // Get current registers
    struct user_regs_struct regs, origRegs;
    if (ptrace(PTRACE_GETREGS, tid, nullptr, &regs) == -1) {
        fprintf(stderr, "[-] Failed to get registers\n");
        Detach(tid);
        return false;
    }
    
    origRegs = regs;
    
    // Allocate memory in target process for library path string
    size_t pathSize = libraryPath.size() + 1;
    uintptr_t pathMem = AllocateMemory(pid, pathSize, PROT_READ | PROT_WRITE);
    
    if (pathMem == 0 || pathMem == (uintptr_t)-1) {
        fprintf(stderr, "[-] Failed to allocate memory for path\n");
        Detach(tid);
        return false;
    }
    
    printf("[+] Allocated path memory at 0x%lX\n", pathMem);
    
    // Write library path
    if (!WriteMemory(pid, pathMem, libraryPath.c_str(), pathSize)) {
        fprintf(stderr, "[-] Failed to write library path\n");
        Detach(tid);
        return false;
    }
    
    // Find dlopen in target process
    // First find libdl.so or libc.so base
    uintptr_t libdlBase = GetModuleBase(pid, "libdl.so");
    uintptr_t libcBase = GetModuleBase(pid, "libc.so");
    
    if (libdlBase == 0 && libcBase == 0) {
        fprintf(stderr, "[-] Failed to find libc/libdl\n");
        Detach(tid);
        return false;
    }
    
    // In a real implementation, we would parse the target's libc to find dlopen
    // For now, we'll use a different approach: LD_PRELOAD style
    
    // Alternatively, use the __libc_dlopen_mode approach
    // This requires finding the symbol in the target process
    
    printf("[*] Library injection requires symbol resolution\n");
    printf("[*] Alternative: Use LD_PRELOAD before starting the game\n");
    
    // Restore registers and detach
    ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
    Detach(tid);
    
    return false; // Full implementation requires symbol resolution
}

//=============================================================================
// Alternative: LD_PRELOAD Method
//=============================================================================

bool SetupPreload(const std::string& libraryPath) {
    // Set LD_PRELOAD environment variable
    setenv("LD_PRELOAD", libraryPath.c_str(), 1);
    return true;
}

} // namespace Injector