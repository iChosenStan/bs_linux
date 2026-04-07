#include "stealth_injector.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <elf.h>
#include <fcntl.h>
#include <fstream>
#include <link.h>
#include <random>
#include <sstream>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

namespace StealthInjector {

//=============================================================================
// Constructor / Destructor
//=============================================================================

StealthInjection::StealthInjection() {}
StealthInjection::~StealthInjection() {}

//=============================================================================
// Helper Functions
//=============================================================================

static std::string ReadFileString(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return "";
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string result = buffer.str();
  // Remove trailing newlines
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
    result.pop_back();
  }
  return result;
}

static std::vector<std::string> SplitString(const std::string &str,
                                            char delimiter) {
  std::vector<std::string> tokens;
  std::istringstream iss(str);
  std::string token;
  while (std::getline(iss, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

static bool FileExists(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

static std::string TrimString(const std::string &str) {
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

std::string StealthInjection::GenerateRandomName(size_t length) {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyz";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

  std::string result;
  result.reserve(length);
  for (size_t i = 0; i < length; i++) {
    result += charset[dis(gen)];
  }
  return result;
}

//=============================================================================
// Process Discovery
//=============================================================================

std::vector<ProcessInfo>
StealthInjection::FindProcesses(const std::string &name) {
  std::vector<ProcessInfo> results;
  std::vector<ProcessInfo> sandboxProcesses; // Store sandbox wrappers for later

  DIR *procDir = opendir("/proc");
  if (!procDir)
    return results;

  struct dirent *entry;
  while ((entry = readdir(procDir)) != nullptr) {
    if (entry->d_type != DT_DIR)
      continue;

    uint32_t pid = 0;
    try {
      pid = std::stoul(entry->d_name);
    } catch (...) {
      continue;
    }

    // Read process name
    std::string comm =
        ReadFileString("/proc/" + std::string(entry->d_name) + "/comm");
    std::string cmdline =
        ReadFileString("/proc/" + std::string(entry->d_name) + "/cmdline");

    // Skip sandbox wrapper processes - we want the actual game inside
    if (comm.find("srt-bwrap") != std::string::npos ||
        comm.find("bwrap") != std::string::npos ||
        comm.find("pressure-vessel") != std::string::npos) {
      // This is a sandbox wrapper - check for children later
      ProcessInfo sandboxInfo = {};
      sandboxInfo.pid = pid;
      sandboxInfo.name = comm;
      sandboxProcesses.push_back(sandboxInfo);
      continue;
    }

    // Check if this matches our target
    bool matches = false;
    if (name.empty()) {
      matches = true;
    } else {
      // Priority 1: Exact match in comm (most reliable)
      if (comm == name) {
        matches = true;
      }
      // Priority 2: Matches .exe process (Windows/Proton games)
      else if (comm.find(name) != std::string::npos &&
               comm.find(".exe") != std::string::npos) {
        matches = true;
      }
      // Priority 3: Check cmdline but ignore the injector's own folder (matches
      // IDE tools)
      else if (cmdline.find(name) != std::string::npos &&
               cmdline.find("Documents/GitHub") == std::string::npos) {
        matches = true;
      }
    }

    if (!matches)
      continue;

    ProcessInfo info = {};
    info.pid = pid;
    info.name = comm;
    info.isWine = IsWineProcess(pid);

    // Get executable path
    char exeBuf[512];
    std::string exePath = "/proc/" + std::string(entry->d_name) + "/exe";
    ssize_t len = readlink(exePath.c_str(), exeBuf, sizeof(exeBuf) - 1);
    if (len > 0) {
      exeBuf[len] = '\0';
      info.path = exeBuf;
    }

    // Get Wine prefix if applicable
    if (info.isWine) {
      info.winePrefix = GetWinePrefix(pid);
    }

    // Prioritize actual game processes (Wine/Proton Windows executables)
    // These will have .exe in their path or be running under Wine
    if (info.isWine || info.path.find(".exe") != std::string::npos ||
        cmdline.find(".exe") != std::string::npos) {
      results.insert(results.begin(), info); // Add to front (higher priority)
    } else {
      results.push_back(info);
    }
  }

  closedir(procDir);

  // If we found sandbox processes but no direct matches, look for child
  // processes
  if (results.empty() && !sandboxProcesses.empty()) {
    for (const auto &sandbox : sandboxProcesses) {
      // Check /proc for children by PPID (skip the useless taskDir scan)
      DIR *procDir2 = opendir("/proc");
      if (procDir2) {
        struct dirent *entry2;
        while ((entry2 = readdir(procDir2)) != nullptr) {
          if (entry2->d_type != DT_DIR)
            continue;

          uint32_t childPid = 0;
          try {
            childPid = std::stoul(entry2->d_name);
          } catch (...) {
            continue;
          }

          // Check if this is a child of the sandbox
          std::string statPath =
              "/proc/" + std::string(entry2->d_name) + "/stat";
          std::ifstream statFile(statPath);
          if (statFile.is_open()) {
            int pid, ppid;
            std::string comm;
            char dummy;
            statFile >> pid >> dummy >> comm >> dummy >> ppid;

            if (ppid == (int)sandbox.pid || ppid == 1) {
              // This could be a child process
              ProcessInfo info = {};
              info.pid = childPid;
              info.name = ReadFileString("/proc/" +
                                         std::string(entry2->d_name) + "/comm");
              info.isWine = IsWineProcess(childPid);

              char exeBuf[512];
              std::string exePath =
                  "/proc/" + std::string(entry2->d_name) + "/exe";
              ssize_t len =
                  readlink(exePath.c_str(), exeBuf, sizeof(exeBuf) - 1);
              if (len > 0) {
                exeBuf[len] = '\0';
                info.path = exeBuf;
              }

              if (info.isWine) {
                info.winePrefix = GetWinePrefix(childPid);
              }

              // Check if this matches our target
              std::string childCmdline = ReadFileString(
                  "/proc/" + std::string(entry2->d_name) + "/cmdline");
              if (info.name.find(name) != std::string::npos ||
                  info.path.find(name) != std::string::npos ||
                  childCmdline.find(name) != std::string::npos || info.isWine) {
                results.push_back(info);
              }
            }
          }
        }
        closedir(procDir2);
      }
    }
  }

  return results;
}

std::optional<ProcessInfo>
StealthInjection::FindProcess(const std::string &name) {
  auto processes = FindProcesses(name);
  if (processes.empty())
    return std::nullopt;
  return processes[0];
}

std::optional<ProcessInfo> StealthInjection::FindProcessByPid(uint32_t pid) {
  std::string commPath = "/proc/" + std::to_string(pid) + "/comm";
  std::string comm = ReadFileString(commPath);

  if (comm.empty())
    return std::nullopt;

  ProcessInfo info = {};
  info.pid = pid;
  info.name = comm;
  info.isWine = IsWineProcess(pid);

  char exeBuf[512];
  std::string exePath = "/proc/" + std::to_string(pid) + "/exe";
  ssize_t len = readlink(exePath.c_str(), exeBuf, sizeof(exeBuf) - 1);
  if (len > 0) {
    exeBuf[len] = '\0';
    info.path = exeBuf;
  }

  if (info.isWine) {
    info.winePrefix = GetWinePrefix(pid);
  }

  return info;
}

std::optional<ProcessInfo>
StealthInjection::FindWineProcess(const std::string &winePrefix,
                                  const std::string &processName) {
  auto processes = FindProcesses(processName);
  for (const auto &proc : processes) {
    if (proc.isWine &&
        (winePrefix.empty() ||
         proc.winePrefix.find(winePrefix) != std::string::npos)) {
      return proc;
    }
  }
  return std::nullopt;
}

//=============================================================================
// Wine/Proton Detection
//=============================================================================

bool StealthInjection::IsWineProcess(uint32_t pid) {
  // Check if process is running under Wine/Proton
  // Method 1: Check exe link for wine/proton path
  char exeBuf[512];
  std::string exePath = "/proc/" + std::to_string(pid) + "/exe";
  ssize_t len = readlink(exePath.c_str(), exeBuf, sizeof(exeBuf) - 1);
  if (len > 0) {
    exeBuf[len] = '\0';
    std::string exe(exeBuf);

    // Check for Proton
    if (exe.find("Proton") != std::string::npos ||
        exe.find("proton") != std::string::npos) {
      return true;
    }

    // Check for Wine
    if (exe.find("wine") != std::string::npos ||
        exe.find("Wine") != std::string::npos) {
      return true;
    }
  }

  // Method 2: Check maps for wine DLLs
  std::string mapsPath = "/proc/" + std::to_string(pid) + "/maps";
  std::ifstream mapsFile(mapsPath);
  if (mapsFile.is_open()) {
    std::string line;
    while (std::getline(mapsFile, line)) {
      if (line.find("wine") != std::string::npos ||
          line.find("ntdll.dll") != std::string::npos ||
          line.find("kernel32.dll") != std::string::npos ||
          line.find("Proton") != std::string::npos) {
        return true;
      }
    }
  }

  // Method 3: Check environment for WINEPREFIX
  std::string environPath = "/proc/" + std::to_string(pid) + "/environ";
  std::ifstream environFile(environPath);
  if (environFile.is_open()) {
    std::stringstream buffer;
    buffer << environFile.rdbuf();
    std::string environ = buffer.str();

    if (environ.find("WINEPREFIX") != std::string::npos ||
        environ.find("STEAM_COMPAT_DATA_PATH") != std::string::npos ||
        environ.find("PROTON") != std::string::npos) {
      return true;
    }
  }

  return false;
}

std::string StealthInjection::GetWinePrefix(uint32_t pid) {
  // Read environment to find WINEPREFIX or Proton path
  std::string environPath = "/proc/" + std::to_string(pid) + "/environ";
  std::ifstream environFile(environPath);
  if (!environFile.is_open())
    return "";

  std::stringstream buffer;
  buffer << environFile.rdbuf();
  std::string environ = buffer.str();

  // Look for WINEPREFIX
  size_t pos = environ.find("WINEPREFIX=");
  if (pos != std::string::npos) {
    size_t start = pos + 11;
    size_t end = environ.find('\0', start);
    if (end != std::string::npos) {
      return environ.substr(start, end - start);
    }
  }

  // Look for STEAM_COMPAT_DATA_PATH (Proton)
  pos = environ.find("STEAM_COMPAT_DATA_PATH=");
  if (pos != std::string::npos) {
    size_t start = pos + 23;
    size_t end = environ.find('\0', start);
    if (end != std::string::npos) {
      return environ.substr(start, end - start);
    }
  }

  return "";
}

uint32_t StealthInjection::GetWineServerPid(const std::string &winePrefix) {
  // Find wineserver process for the given prefix
  DIR *procDir = opendir("/proc");
  if (!procDir)
    return 0;

  struct dirent *entry;
  while ((entry = readdir(procDir)) != nullptr) {
    if (entry->d_type != DT_DIR)
      continue;

    uint32_t pid = 0;
    try {
      pid = std::stoul(entry->d_name);
    } catch (...) {
      continue;
    }

    std::string comm =
        ReadFileString("/proc/" + std::string(entry->d_name) + "/comm");
    if (comm == "wineserver") {
      // Check if this wineserver is for our prefix
      std::string prefix = GetWinePrefix(pid);
      if (!winePrefix.empty() && prefix.find(winePrefix) != std::string::npos) {
        closedir(procDir);
        return pid;
      }
    }
  }

  closedir(procDir);
  return 0;
}

//=============================================================================
// Module Operations
//=============================================================================

std::vector<ModuleInfo> StealthInjection::GetModules(uint32_t pid) {
  std::vector<ModuleInfo> modules;

  std::string mapsPath = "/proc/" + std::to_string(pid) + "/maps";
  std::ifstream mapsFile(mapsPath);
  if (!mapsFile.is_open())
    return modules;

  std::string line;
  std::string lastModule;
  uintptr_t moduleStart = 0;
  uintptr_t moduleEnd = 0;

  while (std::getline(mapsFile, line)) {
    // Parse line: address perms offset dev inode pathname
    std::istringstream iss(line);
    std::string addrRange, perms, offset, dev, inode, pathname;

    iss >> addrRange >> perms >> offset >> dev >> inode;
    std::getline(iss, pathname);
    pathname = TrimString(pathname);

    // Parse address range
    size_t dashPos = addrRange.find('-');
    if (dashPos == std::string::npos)
      continue;

    uintptr_t start = std::stoull(addrRange.substr(0, dashPos), nullptr, 16);
    uintptr_t end = std::stoull(addrRange.substr(dashPos + 1), nullptr, 16);

    // Check if this is a new module
    if (pathname != lastModule) {
      // Save previous module
      if (!lastModule.empty() && moduleStart != 0) {
        ModuleInfo info = {};
        info.path = lastModule;
        info.base = moduleStart;
        info.size = moduleEnd - moduleStart;

        size_t slashPos = lastModule.rfind('/');
        if (slashPos != std::string::npos) {
          info.name = lastModule.substr(slashPos + 1);
        } else {
          info.name = lastModule;
        }

        modules.push_back(info);
      }

      // Reset tracking
      lastModule = pathname;
      moduleStart = start;
    }

    moduleEnd = end;
  }

  // Save last module
  if (!lastModule.empty() && moduleStart != 0) {
    ModuleInfo info = {};
    info.path = lastModule;
    info.base = moduleStart;
    info.size = moduleEnd - moduleStart;

    size_t slashPos = lastModule.rfind('/');
    if (slashPos != std::string::npos) {
      info.name = lastModule.substr(slashPos + 1);
    } else {
      info.name = lastModule;
    }

    modules.push_back(info);
  }

  return modules;
}

std::optional<ModuleInfo> StealthInjection::GetModule(uint32_t pid,
                                                      const std::string &name) {
  auto modules = GetModules(pid);
  for (const auto &mod : modules) {
    if (mod.name.find(name) != std::string::npos ||
        mod.path.find(name) != std::string::npos) {
      return mod;
    }
  }
  return std::nullopt;
}

uintptr_t StealthInjection::GetProcAddress(uint32_t pid, uintptr_t moduleBase,
                                           const std::string &symbol) {
  // Robust ELF symbol resolution
  Elf64_Ehdr ehdr;
  if (!ReadProcessMemory(pid, moduleBase, &ehdr, sizeof(ehdr)))
    return 0;

  if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
    return 0;

  Elf64_Phdr phdr;
  uintptr_t dynamicAddr = 0;
  for (int i = 0; i < ehdr.e_phnum; i++) {
    if (!ReadProcessMemory(pid,
                           moduleBase + ehdr.e_phoff + (i * ehdr.e_phentsize),
                           &phdr, sizeof(phdr)))
      continue;
    if (phdr.p_type == PT_DYNAMIC) {
      dynamicAddr = phdr.p_vaddr;
      break;
    }
  }

  if (dynamicAddr == 0)
    return 0;

  // Some libraries have absolute virtual addresses, some have relative
  if (dynamicAddr < moduleBase)
    dynamicAddr += moduleBase;

  uintptr_t symtab = 0;
  uintptr_t strtab = 0;
  uintptr_t hash = 0;
  uintptr_t gnu_hash = 0;

  Elf64_Dyn dyn;
  uintptr_t currentDyn = dynamicAddr;
  while (true) {
    if (!ReadProcessMemory(pid, currentDyn, &dyn, sizeof(dyn)))
      break;
    if (dyn.d_tag == DT_NULL)
      break;

    switch (dyn.d_tag) {
    case DT_SYMTAB:
      symtab = dyn.d_un.d_ptr;
      break;
    case DT_STRTAB:
      strtab = dyn.d_un.d_ptr;
      break;
    case DT_HASH:
      hash = dyn.d_un.d_ptr;
      break;
    case DT_GNU_HASH:
      gnu_hash = dyn.d_un.d_ptr;
      break;
    }
    currentDyn += sizeof(dyn);
  }

  if (symtab == 0 || strtab == 0)
    return 0;

  // Fix addresses if relative
  if (symtab < moduleBase)
    symtab += moduleBase;
  if (strtab < moduleBase)
    strtab += moduleBase;

  // Search symbol table
  Elf64_Sym sym;
  char nameBuf[256];

  // Limit search to avoid infinite loop on corrupt symtab
  for (int i = 0; i < 5000; i++) {
    if (!ReadProcessMemory(pid, symtab + (i * sizeof(sym)), &sym, sizeof(sym)))
      break;
    if (sym.st_name == 0 && i > 0)
      continue; // Skip empty but not first

    if (!ReadProcessMemory(pid, strtab + sym.st_name, nameBuf, sizeof(nameBuf)))
      continue;
    nameBuf[sizeof(nameBuf) - 1] = '\0';

    if (symbol == nameBuf) {
      // For PIE shared libs st_value is a relative offset; for non-PIE it may
      // already be an absolute VA.  Only add moduleBase when it looks relative.
      if (sym.st_value < moduleBase)
        return moduleBase + sym.st_value;
      return sym.st_value;
    }
  }

  return 0;
}

//=============================================================================
// Memory Operations
//=============================================================================

bool StealthInjection::ReadProcessMemory(uint32_t pid, uintptr_t address,
                                         void *buffer, size_t size) {
  struct iovec local = {.iov_base = buffer, .iov_len = size};

  struct iovec remote = {.iov_base = (void *)address, .iov_len = size};

  ssize_t result = process_vm_readv(pid, &local, 1, &remote, 1, 0);
  return result == static_cast<ssize_t>(size);
}

bool StealthInjection::WriteProcessMemory(uint32_t pid, uintptr_t address,
                                          const void *buffer, size_t size) {
  struct iovec local = {.iov_base = (void *)buffer, .iov_len = size};

  struct iovec remote = {.iov_base = (void *)address, .iov_len = size};

  ssize_t result = process_vm_writev(pid, &local, 1, &remote, 1, 0);
  return result == static_cast<ssize_t>(size);
}

std::vector<uint32_t> StealthInjection::GetThreads(uint32_t pid) {
  std::vector<uint32_t> threads;

  std::string taskPath = "/proc/" + std::to_string(pid) + "/task";
  DIR *taskDir = opendir(taskPath.c_str());
  if (!taskDir)
    return threads;

  struct dirent *entry;
  while ((entry = readdir(taskDir)) != nullptr) {
    // Skip '.' and '..' fast; also skip non-dir entries on DT_UNKNOWN
    // filesystems
    if (entry->d_name[0] == '.')
      continue;
    if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN)
      continue;
    try {
      uint32_t tid = std::stoul(entry->d_name);
      if (tid > 0)
        threads.push_back(tid);
    } catch (...) {
      continue;
    }
  }

  closedir(taskDir);
  return threads;
}

// Internal helper: allocate memory in an already-attached/stopped thread.
// The thread must already be stopped via PTRACE_ATTACH + waitpid before
// calling.
static uintptr_t
AllocateRemoteMemoryInProcess(uint32_t pid, uint32_t tid,
                              const struct user_regs_struct &origRegs,
                              size_t size, int protection) {

  // Find an executable region in the target to temporarily overwrite 3 bytes
  std::string mapsPath = "/proc/" + std::to_string(pid) + "/maps";
  std::ifstream mapsFile(mapsPath);
  uintptr_t codeAddr = 0;

  if (mapsFile.is_open()) {
    std::string line;
    while (std::getline(mapsFile, line)) {
      // Need a readable+executable page we can peek/poke
      if (line.find(" r-xp") != std::string::npos ||
          line.find(" r-xs") != std::string::npos) {
        size_t dashPos = line.find('-');
        if (dashPos != std::string::npos) {
          uintptr_t addr = std::stoull(line.substr(0, dashPos), nullptr, 16);
          // Skip very low addresses (Wine's Windows address space)
          if (addr > 0x7F0000000000ULL) {
            codeAddr = addr;
            break;
          }
        }
      }
    }
    // If nothing found above threshold, fall back to any r-xp region
    if (codeAddr == 0) {
      mapsFile.clear();
      mapsFile.seekg(0);
      std::string line2;
      while (std::getline(mapsFile, line2)) {
        if (line2.find(" r-xp") != std::string::npos ||
            line2.find(" r-xs") != std::string::npos) {
          size_t dashPos = line2.find('-');
          if (dashPos != std::string::npos) {
            codeAddr = std::stoull(line2.substr(0, dashPos), nullptr, 16);
            break;
          }
        }
      }
    }
  }

  if (codeAddr == 0)
    return 0;

  // Backup original 8 bytes at codeAddr
  errno = 0;
  long origWord = ptrace(PTRACE_PEEKTEXT, tid, (void *)codeAddr, nullptr);
  if (errno != 0)
    return 0;

  // Patch: 0x0F 0x05 = syscall, 0xCC = int3
  // Keep upper 5 bytes of the word intact to minimise side-effects
  long trapWord =
      (origWord & (long)0xFFFFFF0000000000LL) | (long)0x00000000CC050FLL;
  if (ptrace(PTRACE_POKETEXT, tid, (void *)codeAddr, (void *)trapWord) == -1) {
    return 0;
  }

  // Set registers for mmap(NULL, size, prot, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
  struct user_regs_struct regs = origRegs;
  regs.rax = SYS_mmap;
  regs.rdi = 0;                           // addr  = NULL
  regs.rsi = (unsigned long)size;         // len
  regs.rdx = (unsigned long)protection;   // prot
  regs.r10 = MAP_PRIVATE | MAP_ANONYMOUS; // flags
  regs.r8 = (unsigned long)(long)-1;      // fd = -1  (must be signed -1)
  regs.r9 = 0;                            // offset
  regs.rip = codeAddr;

  ptrace(PTRACE_SETREGS, tid, nullptr, &regs);

  // Run until int3 fires
  ptrace(PTRACE_CONT, tid, nullptr, nullptr);
  int status = 0;
  waitpid(tid, &status, 0);

  // Get mmap return value
  ptrace(PTRACE_GETREGS, tid, nullptr, &regs);
  uintptr_t result = regs.rax;

  if (result > (uintptr_t)(long)-4096) {
    printf("[-] mmap syscall failed (errno %ld)\n", -(long)result);
    result = 0;
  }

  // Restore original bytes and original registers
  ptrace(PTRACE_POKETEXT, tid, (void *)codeAddr, (void *)origWord);
  ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);

  return result;
}

uintptr_t StealthInjection::AllocateRemoteMemory(uint32_t pid, size_t size,
                                                 int protection) {
  // Standalone version: attach, allocate, detach
  auto threads = GetThreads(pid);
  if (threads.empty())
    return 0;

  uint32_t tid = threads[0];

  if (ptrace(PTRACE_ATTACH, tid, nullptr, nullptr) == -1)
    return 0;

  int status;
  waitpid(tid, &status, 0);
  if (!WIFSTOPPED(status)) {
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return 0;
  }

  struct user_regs_struct origRegs;
  if (ptrace(PTRACE_GETREGS, tid, nullptr, &origRegs) == -1) {
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return 0;
  }

  uintptr_t result =
      AllocateRemoteMemoryInProcess(pid, tid, origRegs, size, protection);

  ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
  return result;
}

bool StealthInjection::CallRemoteFunction(uint32_t pid, uintptr_t function,
                                          uintptr_t *args, size_t argCount,
                                          uintptr_t *result) {
  // This would require more complex ptrace manipulation
  // For now, return false
  return false;
}

//=============================================================================
// Stealth Utilities
//=============================================================================

void StealthInjection::ClearPtraceTraces(uint32_t pid) {
  // Clear TracerPid from /proc/[pid]/status
  // This makes the process appear un-traced

  // We need to modify kernel's view of the process
  // This is highly privileged and may not work on all systems

  // Alternative: Use /proc/[pid]/syscall to hide our traces
  printf("[*] Clearing ptrace traces for PID %u\n", pid);
}

void StealthInjection::HideFromMaps(uint32_t pid, uintptr_t base) {
  // Attempt to hide the injected library from /proc/[pid]/maps
  // This requires kernel-level manipulation or specific techniques

  // Method: Unlink from the process's memory mapping structures
  // This is very advanced and may require kernel module

  printf("[*] Attempting to hide library at 0x%lX from maps\n", base);
}

bool StealthInjection::CheckAntiCheat(uint32_t pid) {
  // Check if target has anti-cheat running
  auto modules = GetModules(pid);

  for (const auto &mod : modules) {
    std::string name = mod.name;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    // Check for common anti-cheat modules
    // NOTE: use full/specific names to avoid false positives (e.g. oleacc.dll
    //       contains "eac" as a substring but is NOT anti-cheat)
    if (name.find("easyanticheat") != std::string::npos || name == "eac.dll" ||
        name == "eac.so" || name.find("battleye") != std::string::npos ||
        name.find("beclient") != std::string::npos ||
        name.find("vanguard") != std::string::npos ||
        name.find("faceit") != std::string::npos ||
        name.find("fairfight") != std::string::npos ||
        name.find("punkbuster") != std::string::npos) {

      printf("[!] Anti-cheat detected: %s\n", mod.name.c_str());
      return true;
    }
  }

  return false;
}

bool StealthInjection::BypassTracerPid() {
  // Try to bypass TracerPid detection
  // This is used by anti-cheats to detect debuggers

  // Method 1: Fork and detach
  pid_t child = fork();
  if (child == 0) {
    // Child process
    _exit(0);
  } else if (child > 0) {
    // Parent: reap child immediately to prevent zombie
    waitpid(child, nullptr, 0);
  }

  return true;
}

//=============================================================================
// Library Loading
//=============================================================================

std::vector<uint8_t>
StealthInjection::ReadLibraryFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return {};
  }

  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char *>(data.data()), size);

  return data;
}

//=============================================================================
// Injection Methods
//=============================================================================

bool StealthInjection::InjectPtrace(uint32_t pid,
                                    const std::string &libraryPath,
                                    InjectionResult &result) {
  printf("[*] Using ptrace injection method\n");

  if (!FileExists(libraryPath)) {
    result.error = "Library file not found: " + libraryPath;
    return false;
  }

  auto threads = GetThreads(pid);
  if (threads.empty()) {
    result.error = "Failed to get threads";
    return false;
  }

  // === Smart thread selection: prefer interruptibly-sleeping threads ===
  // A sleeping thread (State: S) is safe to hijack — it's blocked in a syscall
  // and holds no CPU-level locks. Running/uninterruptible threads can deadlock
  // if they hold the dl_load_lock or heap lock when dlopen is called.
  uint32_t tid = 0;
  for (uint32_t t : threads) {
    std::string stPath = "/proc/" + std::to_string(pid) + "/task/" +
                         std::to_string(t) + "/status";
    std::ifstream sf(stPath);
    std::string line;
    while (std::getline(sf, line)) {
      if (line.rfind("State:", 0) == 0) {
        if (line.find('S') != std::string::npos) {
          tid = t;
        }
        break;
      }
    }
    if (tid)
      break;
  }
  if (!tid)
    tid = threads[0]; // Fallback to first thread
  printf("[+] Selected thread %u for injection\n", tid);

  // === Attach once — stay attached for all operations ===
  if (ptrace(PTRACE_ATTACH, tid, nullptr, nullptr) == -1) {
    result.error = std::string("Failed to attach to thread ") +
                   std::to_string(tid) + " (" + strerror(errno) + ")";
    return false;
  }

  int status = 0;
  waitpid(tid, &status, 0);
  if (!WIFSTOPPED(status)) {
    result.error = "Thread did not stop after PTRACE_ATTACH";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  printf("[+] Attached to thread %u\n", tid);

  struct user_regs_struct origRegs;
  if (ptrace(PTRACE_GETREGS, tid, nullptr, &origRegs) == -1) {
    result.error = "Failed to get registers";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }

  // Find libc + dlopen
  auto modules = GetModules(pid);
  printf("[*] Found %zu modules in process %u\n", modules.size(), pid);
  for (const auto &mod : modules) {
    printf("    0x%12lX - %s\n", mod.base, mod.name.c_str());
  }

  ModuleInfo libcMod = {};
  for (const auto &mod : modules) {
    std::string ln = mod.name, lp = mod.path;
    std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
    std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
    if (ln.find("libc.so") == 0 || ln.find("libc-") == 0 ||
        lp.find("/libc.so") != std::string::npos ||
        lp.find("/libc-") != std::string::npos) {
      libcMod = mod;
      break;
    }
  }
  if (libcMod.base == 0) {
    result.error = "Failed to find libc in target process";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  printf("[+] Found libc at 0x%lX\n", libcMod.base);

  uintptr_t dlopenAddr = GetProcAddress(pid, libcMod.base, "dlopen");
  if (!dlopenAddr)
    dlopenAddr = GetProcAddress(pid, libcMod.base, "__libc_dlopen_mode");
  if (!dlopenAddr) {
    for (const auto &mod : modules) {
      std::string ln = mod.name, lp = mod.path;
      std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
      std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
      if (ln.find("libdl.so") == 0 || ln.find("libdl-") == 0 ||
          lp.find("/libdl.so") != std::string::npos ||
          lp.find("/libdl-") != std::string::npos) {
        dlopenAddr = GetProcAddress(pid, mod.base, "dlopen");
        if (dlopenAddr)
          break;
      }
    }
  }
  if (!dlopenAddr) {
    result.error = "Failed to find dlopen in target process";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  printf("[+] Found dlopen at 0x%lX\n", dlopenAddr);

  uintptr_t dlerrorAddr = GetProcAddress(pid, libcMod.base, "dlerror");
  if (!dlerrorAddr) {
    for (const auto &mod : modules) {
      std::string ln = mod.name, lp = mod.path;
      std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
      std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
      if (ln.find("libdl.so") == 0 || ln.find("libdl-") == 0 ||
          lp.find("/libdl.so") != std::string::npos ||
          lp.find("/libdl-") != std::string::npos) {
        dlerrorAddr = GetProcAddress(pid, mod.base, "dlerror");
        if (dlerrorAddr)
          break;
      }
    }
  }
  if (dlerrorAddr) {
    printf("[+] Found dlerror at 0x%lX\n", dlerrorAddr);
  } else {
    result.warnings.push_back("dlerror symbol not found; dlopen failures will be less descriptive");
  }

  // === Allocate a single 8KB block for shellcode, path, and dedicated stack
  // === This fixes SIGSEGV crashes by providing a clean, isolated stack for
  // dlopen that doesn't interfere with the target thread's original stack.
  //
  // Memory layout (8KB total):
  //   [0x000 - 0x0FF] Shellcode (256 bytes) - executable code to call dlopen
  //   [0x100 - 0x1FF] Path (256 bytes) - library path string
  //   [0x200 - 0x1FFF] Stack (~7.5KB) - dedicated stack for dlopen execution
  //
  // RSP will point to the TOP of the stack area (0x2000) for proper growth
  // downward.

  constexpr size_t kAllocSize = 8192; // 8KB allocation
  constexpr size_t kShellcodeOffset = 0;
  constexpr size_t kShellcodeSize = 256;
  constexpr size_t kPathOffset = 256;
  constexpr size_t kPathSize = 256;
  constexpr size_t kDataOffset = 512;
  constexpr size_t kStackOffset = 768;

  uintptr_t blockBase = AllocateRemoteMemoryInProcess(
      pid, tid, origRegs, kAllocSize, PROT_READ | PROT_WRITE | PROT_EXEC);
  if (!blockBase) {
    result.error = "Failed to allocate 8KB block for injection";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  printf("[+] Allocated 8KB block at 0x%lX\n", blockBase);

  uintptr_t shellcodeMem = blockBase + kShellcodeOffset;
  uintptr_t pathMem = blockBase + kPathOffset;
  uintptr_t dataMem = blockBase + kDataOffset;
  uintptr_t stackTop = blockBase + kAllocSize; // Top of the 8KB block

  // Write the library path into the allocated block
  size_t pathSize = libraryPath.size() + 1;
  if (pathSize > kPathSize) {
    result.error = "Library path too long (max " +
                   std::to_string(kPathSize - 1) + " chars)";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  if (!WriteProcessMemory(pid, pathMem, libraryPath.c_str(), pathSize)) {
    result.error = "Failed to write library path";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  printf("[+] Path at 0x%lX: %s\n", pathMem, libraryPath.c_str());

  const size_t kDlopenShellcodeBytes = 30;
  const size_t kDlopenInt3Offset = 29;
  uintptr_t int3Addr = shellcodeMem + kDlopenInt3Offset;

  uint8_t shellcode[kDlopenShellcodeBytes];
  memset(shellcode, 0x90, sizeof(shellcode));

  // 2-byte NOP prefix (helps when RIP is adjusted by -2 on resume)
  shellcode[0] = 0x66;
  shellcode[1] = 0x90;

  // movabs rax, dlopenAddr
  shellcode[2] = 0x48;
  shellcode[3] = 0xB8;
  memcpy(&shellcode[4], &dlopenAddr, 8);

  // movabs rdi, pathMem
  shellcode[12] = 0x48;
  shellcode[13] = 0xBF;
  memcpy(&shellcode[14], &pathMem, 8);

  // mov esi, 2
  shellcode[22] = 0xBE;
  shellcode[23] = 0x02;
  shellcode[24] = 0x00;
  shellcode[25] = 0x00;
  shellcode[26] = 0x00;

  // call rax
  shellcode[27] = 0xFF;
  shellcode[28] = 0xD0;

  // int3
  shellcode[kDlopenInt3Offset] = 0xCC;

  // Write shellcode into the allocated block
  if (!WriteProcessMemory(pid, shellcodeMem, shellcode, sizeof(shellcode))) {
    result.error = "Failed to write shellcode";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  printf("[+] Shellcode at 0x%lX (%zu bytes), int3 at 0x%lX\n", shellcodeMem,
         sizeof(shellcode), (unsigned long)int3Addr);

  // === Execute shellcode with dedicated stack ===
  // CRITICAL: Use our own dedicated stack to avoid SIGSEGV crashes.
  //   1. Set RSP to the top of our allocated stack area (grows downward).
  //   2. The stack is 16-byte aligned at the top (8KB is divisible by 16).
  //   3. Write the address of int3 as return address so dlopen returns to our
  //   trap.
  //   4. The dedicated stack provides ~7.5KB of clean stack space for dlopen.
  struct user_regs_struct regs = origRegs;
  regs.rsp = stackTop; // Point to top of our dedicated stack
  regs.rsp = (regs.rsp - 8) &
             ~(uintptr_t)0xFULL; // Align to 16 bytes and reserve space
  // Write the return address (int3) to the stack
  uintptr_t returnAddr = int3Addr;
  if (!WriteProcessMemory(pid, regs.rsp, &returnAddr, sizeof(returnAddr))) {
    result.error = "Failed to write return address to stack";
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }
  regs.rip = shellcodeMem + 2;
  // Zero caller-saved registers that dlopen doesn't expect to be arbitrary
  // values
  regs.rcx = regs.r10 = regs.r11 = 0;
  // Set RBP to 0 to indicate bottom of stack frame
  regs.rbp = 0;
  printf("[+] Setting RSP to 0x%lX (dedicated stack), RIP to 0x%lX\n",
         (unsigned long)regs.rsp, (unsigned long)regs.rip);

  if (ptrace(PTRACE_SETREGS, tid, nullptr, &regs) == -1) {
    result.error = std::string("Failed to set registers: ") + strerror(errno);
    ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }

  // Verify registers applied
  struct user_regs_struct verifyRegs = {};
  if (ptrace(PTRACE_GETREGS, tid, nullptr, &verifyRegs) == 0) {
    if (verifyRegs.rip != regs.rip || verifyRegs.rsp != regs.rsp) {
      char buf[256];
      snprintf(buf, sizeof(buf),
               "Register mismatch after SETREGS | RIP=0x%016llX (want 0x%016llX) "
               "RSP=0x%016llX (want 0x%016llX)",
               (unsigned long long)verifyRegs.rip, (unsigned long long)regs.rip,
               (unsigned long long)verifyRegs.rsp, (unsigned long long)regs.rsp);
      result.error = buf;
      ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
      ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
      return false;
    }
  }

  ptrace(PTRACE_CONT, tid, nullptr, nullptr);

  status = 0;
  waitpid(tid, &status, 0);
  if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
    // Collect registers for diagnostics before we detach
    struct user_regs_struct crashRegs = {};
    ptrace(PTRACE_GETREGS, tid, nullptr, &crashRegs);

    // Collect siginfo (fault address etc.)
    siginfo_t si = {};
    bool haveSigInfo = (ptrace(PTRACE_GETSIGINFO, tid, nullptr, &si) == 0);

    // Read back first 32 bytes of shellcode for verification
    uint8_t shellDump[32];
    memset(shellDump, 0, sizeof(shellDump));
    bool haveShell = ReadProcessMemory(pid, shellcodeMem, shellDump, sizeof(shellDump));
    char shellHex[128];
    shellHex[0] = '\0';
    if (haveShell) {
      size_t w = 0;
      for (size_t k = 0; k < sizeof(shellDump) && w + 4 < sizeof(shellHex); k++) {
        w += snprintf(shellHex + w, sizeof(shellHex) - w, "%02X ", shellDump[k]);
      }
    }

    char diagBuf[256];
    if (haveSigInfo) {
      snprintf(diagBuf, sizeof(diagBuf),
               "Shellcode execution didn't stop at int3 (signal=%d code=%d addr=0x%llX) "
               "| RIP=0x%016llX RSP=0x%016llX shellcode=0x%016llX",
               WSTOPSIG(status), si.si_code, (unsigned long long)(uintptr_t)si.si_addr,
               (unsigned long long)crashRegs.rip,
               (unsigned long long)crashRegs.rsp,
               (unsigned long long)shellcodeMem);
    } else {
      snprintf(diagBuf, sizeof(diagBuf),
               "Shellcode execution didn't stop at int3 (signal=%d) "
               "| RIP=0x%016llX RSP=0x%016llX shellcode=0x%016llX",
               WSTOPSIG(status), (unsigned long long)crashRegs.rip,
               (unsigned long long)crashRegs.rsp,
               (unsigned long long)shellcodeMem);
    }
    result.error = diagBuf;

    if (haveShell) {
      result.warnings.push_back(std::string("Shellcode bytes: ") + shellHex);
    }
    ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }

  ptrace(PTRACE_GETREGS, tid, nullptr, &regs);
  if (regs.rax == 0) {
    std::string dlerr;
    if (dlerrorAddr) {
      // Clear data slot used to pass dlerror() pointer back
      uintptr_t zero = 0;
      WriteProcessMemory(pid, dataMem, &zero, sizeof(zero));

      // Stage 2: call dlerror() and store pointer into dataMem, then int3.
      constexpr size_t kDlerrorShellcodeOffset = 128;
      uintptr_t dlerrorShellcodeMem = shellcodeMem + kDlerrorShellcodeOffset;
      const size_t kDlerrorShellcodeBytes = 34;

      uint8_t sc2[kDlerrorShellcodeBytes];
      memset(sc2, 0x90, sizeof(sc2));

      // 2-byte NOP prefix (helps when RIP is adjusted by -2 on resume)
      sc2[0] = 0x66;
      sc2[1] = 0x90;

      // movabs rax, dlerrorAddr
      sc2[2] = 0x48;
      sc2[3] = 0xB8;
      memcpy(&sc2[4], &dlerrorAddr, 8);
      // call rax
      sc2[12] = 0xFF;
      sc2[13] = 0xD0;
      // movabs rbx, dataMem
      sc2[14] = 0x48;
      sc2[15] = 0xBB;
      memcpy(&sc2[16], &dataMem, 8);
      // mov [rbx], rax
      sc2[24] = 0x48;
      sc2[25] = 0x89;
      sc2[26] = 0x03;
      // int3
      sc2[kDlerrorShellcodeBytes - 1] = 0xCC;

      if (!WriteProcessMemory(pid, dlerrorShellcodeMem, sc2, sizeof(sc2))) {
        result.error = "dlopen returned NULL in target process (and failed to write dlerror shellcode)";
        ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
        ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
        return false;
      }

      // Reuse the dedicated stack
      struct user_regs_struct regs2 = origRegs;
      regs2.rsp = stackTop;
      regs2.rsp = (regs2.rsp - 8) & ~(uintptr_t)0xFULL;
      // Provide a valid return address even though our shellcode never returns.
      uintptr_t returnAddr2 = dlerrorShellcodeMem + (kDlerrorShellcodeBytes - 1);
      if (!WriteProcessMemory(pid, regs2.rsp, &returnAddr2, sizeof(returnAddr2))) {
        result.error = "dlopen returned NULL in target process (and failed to set dlerror return address)";
        ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
        ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
        return false;
      }
      regs2.rip = dlerrorShellcodeMem + 2;
      regs2.rcx = regs2.r10 = regs2.r11 = 0;
      regs2.rbp = 0;
      ptrace(PTRACE_SETREGS, tid, nullptr, &regs2);
      ptrace(PTRACE_CONT, tid, nullptr, nullptr);

      int status2 = 0;
      waitpid(tid, &status2, 0);
      if (!WIFSTOPPED(status2) || WSTOPSIG(status2) != SIGTRAP) {
        struct user_regs_struct s2Regs = {};
        ptrace(PTRACE_GETREGS, tid, nullptr, &s2Regs);
        siginfo_t si2 = {};
        bool haveSi2 = (ptrace(PTRACE_GETSIGINFO, tid, nullptr, &si2) == 0);
        char buf[256];
        if (haveSi2) {
          snprintf(buf, sizeof(buf),
                   "dlerror stage2 didn't stop at int3 (signal=%d code=%d addr=0x%llX) | RIP=0x%016llX",
                   WSTOPSIG(status2), si2.si_code, (unsigned long long)(uintptr_t)si2.si_addr,
                   (unsigned long long)s2Regs.rip);
        } else {
          snprintf(buf, sizeof(buf),
                   "dlerror stage2 didn't stop at int3 (signal=%d) | RIP=0x%016llX",
                   WSTOPSIG(status2), (unsigned long long)s2Regs.rip);
        }
        result.warnings.push_back(buf);
      }

      uintptr_t errPtr = 0;
      if (ReadProcessMemory(pid, dataMem, &errPtr, sizeof(errPtr))) {
        if (errPtr) {
          char errBuf[512];
          memset(errBuf, 0, sizeof(errBuf));
          if (ReadProcessMemory(pid, errPtr, errBuf, sizeof(errBuf) - 1)) {
            dlerr = errBuf;
          } else {
            result.warnings.push_back("dlerror pointer captured but failed to read error string");
          }
        } else {
          result.warnings.push_back("dlerror returned NULL or did not set error string");
        }
      } else {
        result.warnings.push_back("Failed to read dlerror pointer from target");
      }
    }

    if (!dlerr.empty())
      result.error = std::string("dlopen returned NULL in target process: ") + dlerr;
    else
      result.error = "dlopen returned NULL in target process";
    ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
    ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    return false;
  }

  printf("[+] Library loaded successfully! Handle: 0x%" PRIxPTR "\n",
         (uintptr_t)regs.rax);

  ptrace(PTRACE_SETREGS, tid, nullptr, &origRegs);
  ptrace(PTRACE_DETACH, tid, nullptr, nullptr);

  result.success = true;
  return true;
}

bool StealthInjection::InjectMemfd(uint32_t pid,
                                   const std::vector<uint8_t> &libraryData,
                                   InjectionResult &result) {
  printf("[*] Using memfd injection method\n");

  // Strategy: Use memfd_create to create an anonymous in-memory file.
  // We use /proc/self/fd/<N> to allow the target to access it across privilege
  // boundaries.

  // 1. Create anonymous fd
  int memfd = static_cast<int>(syscall(SYS_memfd_create, "libbloodstrike", 0));
  if (memfd == -1) {
    result.error = std::string("memfd_create failed: ") + strerror(errno);
    return false;
  }

  // 2. Write library bytes into memfd (loop to handle partial writes)
  size_t written = 0;
  while (written < libraryData.size()) {
    ssize_t n = write(memfd, libraryData.data() + written,
                      libraryData.size() - written);
    if (n <= 0) {
      result.error = std::string("write to memfd failed: ") + strerror(errno);
      close(memfd);
      return false;
    }
    written += static_cast<size_t>(n);
  }
  printf("[+] Memfd loaded: %zu bytes (no disk footprint)\n", written);

  // 3. Use /proc/self/fd/<N> which is accessible across privilege boundaries
  //    when the target process follows the symlink. This works because:
  //    - The injector creates the memfd and keeps it open
  //    - The target process can read /proc/<injector_pid>/fd/<N> if it has read
  //    access to /proc
  //    - We use the injector's actual PID (not getpid() which might be
  //    different after pkexec)
  char memfdPath[128];
  snprintf(memfdPath, sizeof(memfdPath), "/proc/%d/fd/%d",
           static_cast<int>(getpid()), memfd);
  printf("[+] Ephemeral path: %s\n", memfdPath);

  // 4. Use ptrace injection with this ephemeral path (dlopen on target side)
  bool ok = InjectPtrace(pid, memfdPath, result);

  // 5. Close our fd. The library is already mapped in the target via dlopen
  //    which holds its own reference, so this does NOT unload it.
  close(memfd);

  if (ok)
    printf("[+] Memfd injection successful (no disk footprint)\n");
  return ok;
}

bool StealthInjection::InjectPreload(const std::string &libraryPath,
                                     const std::string &targetCmd) {
  printf("[*] Using LD_PRELOAD injection method\n");

  // Set LD_PRELOAD environment variable
  setenv("LD_PRELOAD", libraryPath.c_str(), 1);

  printf("[+] Set LD_PRELOAD=%s\n", libraryPath.c_str());
  printf("[*] Library will be loaded into any new process\n");

  return true;
}

bool StealthInjection::InjectWine(uint32_t pid, const std::string &libraryPath,
                                  InjectionResult &result) {
  printf("[*] Using Wine/Proton injection method\n");

  std::string winePrefix = GetWinePrefix(pid);
  printf("[+] Wine prefix: %s\n", winePrefix.c_str());

  // For Wine/Proton, we have several options:
  // 1. Use wineserver to inject
  // 2. Use LD_PRELOAD before starting the game
  // 3. Use Windows-style DLL injection
  // 4. Use process_vm_writev + dlopen

  // Check if library is a Windows DLL or Linux .so
  bool isWindowsDll = libraryPath.find(".dll") != std::string::npos;

  if (isWindowsDll) {
    // Windows DLL injection into Wine process
    printf("[*] Injecting Windows DLL into Wine process\n");

    // We would use Wine's internal injection mechanisms
    // This typically involves using wineserver or winedbg

    result.error = "Windows DLL injection not yet implemented";
    return false;
  }

  // Linux .so injection into Wine process
  printf("[*] Injecting Linux .so into Wine process\n");

  // First, try to find the ACTUAL game process (not sandbox wrapper)
  auto modules = GetModules(pid);
  printf("[*] Process has %zu loaded modules\n", modules.size());

  // Check if this looks like a proper Wine process with loaded DLLs
  bool hasWineDlls = false;
  for (const auto &mod : modules) {
    if (mod.name.find("ntdll") != std::string::npos ||
        mod.name.find("kernel32") != std::string::npos ||
        mod.name.find("wine") != std::string::npos) {
      hasWineDlls = true;
      printf("[+] Found Wine module: %s at 0x%lX\n", mod.name.c_str(),
             mod.base);
    }
  }

  // Check for libc (needed for ptrace injection).
  // Use "libc.so" to avoid matching libcap, libcompiler-rt, libclang-rt, etc.
  bool hasLibc = false;
  for (const auto &mod : modules) {
    if (mod.name.find("libc.so") != std::string::npos ||
        mod.name.find("libc-") != std::string::npos) {
      hasLibc = true;
      printf("[+] Found libc: %s at 0x%lX\n", mod.name.c_str(), mod.base);
      break;
    }
  }

  // If no Wine DLLs or libc, this might be a sandbox wrapper
  if (!hasWineDlls && !hasLibc) {
    printf("[!] Target appears to be a sandbox wrapper, not the actual game\n");
    printf("[!] Looking for actual game process...\n");

    // Try to find child processes
    DIR *procDir = opendir("/proc");
    if (procDir) {
      struct dirent *entry;
      while ((entry = readdir(procDir)) != nullptr) {
        if (entry->d_type != DT_DIR)
          continue;

        uint32_t childPid = 0;
        try {
          childPid = std::stoul(entry->d_name);
        } catch (...) {
          continue;
        }

        // Skip our own process
        if (childPid == pid)
          continue;

        // Check if this is a Wine process
        if (IsWineProcess(childPid)) {
          auto childModules = GetModules(childPid);
          for (const auto &mod : childModules) {
            if (mod.name.find("BloodStrike") != std::string::npos ||
                mod.name.find("bloodstrike") != std::string::npos ||
                mod.path.find("BloodStrike") != std::string::npos) {
              printf("[+] Found actual game process: PID %u\n", childPid);
              printf("[+] Switching to game process for injection\n");
              pid = childPid;
              modules = childModules;
              hasWineDlls = true;
              hasLibc = false;
              // Check for libc in new process
              for (const auto &mod : modules) {
                if (mod.name.find("libc.so") != std::string::npos ||
                    mod.name.find("libc-") != std::string::npos) {
                  hasLibc = true;
                  break;
                }
              }
              break;
            }
          }
          if (hasWineDlls)
            break;
        }
      }
      closedir(procDir);
    }
  }

  if (!hasLibc) {
    printf("[-] Cannot find libc in target process\n");
    printf(
        "[!] The game may be running in a sandbox that prevents injection\n");
    printf("\n");
    printf("=== Alternative Methods ===\n");
    printf("1. LD_PRELOAD method (requires game restart):\n");
    printf("   LD_PRELOAD=%s steam://rungameid/<gameid>\n",
           libraryPath.c_str());
    printf("\n");
    printf("2. For Proton games, add to Steam launch options:\n");
    printf("   LD_PRELOAD=$PWD/libbloodstrike.so %%command%%\n");
    printf("\n");
    printf("3. Use GDB for manual injection:\n");
    printf("   gdb -p <pid>\n");
    printf("   (gdb) call (void*)dlopen(\"%s\", 1)\n", libraryPath.c_str());

    result.error = "Cannot inject into sandbox - use LD_PRELOAD method";
    result.warnings.push_back(
        "For Wine/Proton, LD_PRELOAD before game start is recommended");
    return false;
  }

  // Try ptrace injection if we have libc
  printf("[*] Using ptrace injection method\n");
  return InjectPtrace(pid, libraryPath, result);
}

//=============================================================================
// Main Injection Entry Point
//=============================================================================

InjectionResult StealthInjection::Inject(const InjectionConfig &config) {
  InjectionResult result = {};
  result.success = false;

  printf("==============================================\n");
  printf("  Stealth Injector v1.0\n");
  printf("==============================================\n\n");

  // Validate library path
  if (!FileExists(config.libraryPath)) {
    result.error = "Library file not found: " + config.libraryPath;
    return result;
  }

  printf("[+] Library: %s\n", config.libraryPath.c_str());

  // Find target process
  std::optional<ProcessInfo> procInfo;

  if (config.targetPid != 0) {
    procInfo = FindProcessByPid(config.targetPid);
  } else if (!config.targetProcess.empty()) {
    if (!config.winePrefix.empty()) {
      procInfo = FindWineProcess(config.winePrefix, config.targetProcess);
    } else {
      procInfo = FindProcess(config.targetProcess);
    }
  }

  if (!procInfo) {
    result.error = "Target process not found: " +
                   (config.targetPid ? std::to_string(config.targetPid)
                                     : config.targetProcess);
    return result;
  }

  result.injectedPid = procInfo->pid;

  printf("[+] Target: %s (PID: %u)\n", procInfo->name.c_str(), procInfo->pid);
  printf("[+] Path: %s\n", procInfo->path.c_str());
  printf("[+] Wine/Proton: %s\n", procInfo->isWine ? "Yes" : "No");

  if (procInfo->isWine) {
    printf("[+] Wine Prefix: %s\n", procInfo->winePrefix.c_str());
  }

  // Check for anti-cheat
  if (CheckAntiCheat(procInfo->pid)) {
    result.warnings.push_back(
        "Anti-cheat detected - injection may be detected");

    if (config.verbose) {
      printf("[!] Warning: Anti-cheat detected in target process\n");
      printf("[!] Injection may be detected and blocked\n");
    }
  }

  // Choose injection method
  int method = config.injectionMethod;

  if (method == 0) {
    // Auto-select method
    if (procInfo->isWine) {
      method = 4; // Wine-specific
    } else {
      method = 1; // Ptrace
    }
  }

  printf("[*] Using injection method: ");
  switch (method) {
  case 1:
    printf("Ptrace\n");
    break;
  case 2:
    printf("Memfd\n");
    break;
  case 3:
    printf("LD_PRELOAD\n");
    break;
  case 4:
    printf("Wine/Proton\n");
    break;
  default:
    printf("Auto\n");
    break;
  }

  // Execute injection
  switch (method) {
  case 1:
    result.success = InjectPtrace(procInfo->pid, config.libraryPath, result);
    break;

  case 2: {
    auto libData = ReadLibraryFile(config.libraryPath);
    if (libData.empty()) {
      result.error = "Failed to read library file";
    } else {
      result.success = InjectMemfd(procInfo->pid, libData, result);
    }
    break;
  }

  case 3:
    result.success = InjectPreload(config.libraryPath, "");
    break;

  case 4:
    result.success = InjectWine(procInfo->pid, config.libraryPath, result);
    break;

  default:
    result.error = "Invalid injection method";
    break;
  }

  return result;
}

//=============================================================================
// Quick Injection Functions
//=============================================================================

bool QuickInject(const std::string &processName,
                 const std::string &libraryPath) {
  StealthInjection injector;

  InjectionConfig config = {};
  config.targetProcess = processName;
  config.libraryPath = libraryPath;
  config.injectionMethod = 0; // Auto

  auto result = injector.Inject(config);

  if (!result.success) {
    printf("[-] Injection failed: %s\n", result.error.c_str());
    for (const auto &warn : result.warnings) {
      printf("[!] Warning: %s\n", warn.c_str());
    }
  } else {
    printf("[+] Injection successful!\n");
  }

  return result.success;
}

bool InjectProton(const std::string &processName,
                  const std::string &libraryPath,
                  const std::string &protonPrefix) {
  StealthInjection injector;

  InjectionConfig config = {};
  config.targetProcess = processName;
  config.libraryPath = libraryPath;
  config.winePrefix = protonPrefix;
  config.injectionMethod = 4; // Wine/Proton method

  auto result = injector.Inject(config);

  if (!result.success) {
    printf("[-] Injection failed: %s\n", result.error.c_str());
    for (const auto &warn : result.warnings) {
      printf("[!] Warning: %s\n", warn.c_str());
    }
  } else {
    printf("[+] Injection successful!\n");
  }

  return result.success;
}

} // namespace StealthInjector