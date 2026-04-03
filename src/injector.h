#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Injector {

// Process information
struct ProcessInfo {
    uint32_t pid;
    std::string name;
    std::string path;
    uintptr_t baseAddress;
    size_t moduleSize;
};

// Find process by name
ProcessInfo FindProcess(const std::string& processName);

// Get module base address
uintptr_t GetModuleBase(uint32_t pid, const std::string& moduleName);

// Inject shared library into process
bool InjectLibrary(uint32_t pid, const std::string& libraryPath);

// Read process memory
bool ReadMemory(uint32_t pid, uintptr_t address, void* buffer, size_t size);

// Write process memory
bool WriteMemory(uint32_t pid, uintptr_t address, const void* buffer, size_t size);

// Allocate memory in remote process
uintptr_t AllocateMemory(uint32_t pid, size_t size, int protection);

// Execute code in remote process (for shellcode injection)
bool ExecuteRemote(uint32_t pid, uintptr_t address);

// Attach to process (ptrace)
bool Attach(uint32_t pid);

// Detach from process
void Detach(uint32_t pid);

// Get thread IDs for a process
std::vector<uint32_t> GetThreads(uint32_t pid);

} // namespace Injector