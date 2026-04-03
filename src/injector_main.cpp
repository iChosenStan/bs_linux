#include <iostream>
#include <string>
#include <cstring>
#include <getopt.h>
#include "stealth_injector.h"

void PrintBanner() {
    printf(R"(
╔══════════════════════════════════════════════════════════════╗
║                   STEALTH INJECTOR v1.0                      ║
║              Undetectable Linux Code Injection               ║
║                   Proton/Wine Compatible                     ║
╚══════════════════════════════════════════════════════════════╝
)");
}

void PrintUsage(const char* programName) {
    printf("\nUsage: %s [OPTIONS] -p <process|pid> -l <library>\n\n", programName);
    printf("Options:\n");
    printf("  -p, --process <name|pid>  Target process name or PID\n");
    printf("  -l, --library <path>      Library to inject (.so or .dll)\n");
    printf("  -m, --method <1-4>        Injection method:\n");
    printf("                              1 = Ptrace (default for native)\n");
    printf("                              2 = Memfd (anonymous memory)\n");
    printf("                              3 = LD_PRELOAD (requires restart)\n");
    printf("                              4 = Wine/Proton (for Windows games)\n");
    printf("  -w, --wine-prefix <path>  Wine/Proton prefix path\n");
    printf("  -L, --list                List matching processes\n");
    printf("  -s, --status <pid>        Show process status (Wine check, modules)\n");
    printf("  -v, --verbose             Verbose output\n");
    printf("  -h, --help                Show this help\n");
    printf("\nExamples:\n");
    printf("  %s -p bloodstrike -l ./libbloodstrike.so\n", programName);
    printf("  %s -p 12345 -l ./libbloodstrike.so -m 4\n", programName);
    printf("  %s -p bloodstrike -l ./cheat.dll -w ~/.steam/proton...\n", programName);
    printf("  %s -L -p bloodstrike\n", programName);
    printf("\n");
}

void ListProcesses(const std::string& name) {
    using namespace StealthInjector;
    
    StealthInjection injector;
    auto processes = injector.FindProcesses(name);
    
    if (processes.empty()) {
        printf("No processes found matching '%s'\n", name.c_str());
        return;
    }
    
    printf("\n%-8s %-24s %-8s %s\n", "PID", "NAME", "WINE", "PATH");
    printf("%-8s %-24s %-8s %s\n", "---", "----", "----", "----");
    
    for (const auto& proc : processes) {
        printf("%-8u %-24s %-8s %s\n", 
            proc.pid, 
            proc.name.c_str(),
            proc.isWine ? "Yes" : "No",
            proc.path.c_str());
        
        if (proc.isWine && !proc.winePrefix.empty()) {
            printf("         Wine Prefix: %s\n", proc.winePrefix.c_str());
        }
    }
    printf("\n");
}

void ShowProcessStatus(uint32_t pid) {
    using namespace StealthInjector;
    
    StealthInjection injector;
    auto procInfo = injector.FindProcessByPid(pid);
    
    if (!procInfo) {
        printf("Process with PID %u not found\n", pid);
        return;
    }
    
    printf("\n=== Process Status ===\n");
    printf("PID:          %u\n", procInfo->pid);
    printf("Name:         %s\n", procInfo->name.c_str());
    printf("Path:         %s\n", procInfo->path.c_str());
    printf("Wine/Proton:  %s\n", procInfo->isWine ? "Yes" : "No");
    
    if (procInfo->isWine) {
        printf("Wine Prefix:  %s\n", procInfo->winePrefix.c_str());
    }
    
    // List modules
    printf("\n=== Loaded Modules (first 20) ===\n");
    auto modules = injector.GetModules(pid);
    
    int count = 0;
    for (const auto& mod : modules) {
        if (count++ >= 20) {
            printf("... and %zu more modules\n", modules.size() - 20);
            break;
        }
        printf("0x%012lX - 0x%012lX  %s\n", mod.base, mod.base + mod.size, mod.name.c_str());
    }
    
    // Check for anti-cheat
    printf("\n=== Anti-Cheat Check ===\n");
    bool acDetected = injector.CheckAntiCheat(pid);
    if (acDetected) {
        printf("[!] Anti-cheat modules detected!\n");
    } else {
        printf("[+] No anti-cheat modules detected\n");
    }
    
    printf("\n");
}

int main(int argc, char* argv[]) {
    PrintBanner();
    
    std::string processName;
    std::string libraryPath;
    std::string winePrefix;
    int method = 0; // Auto
    bool verbose = false;
    bool listMode = false;
    bool statusMode = false;
    uint32_t targetPid = 0;
    
    static struct option longOptions[] = {
        {"process",     required_argument, 0, 'p'},
        {"library",     required_argument, 0, 'l'},
        {"method",      required_argument, 0, 'm'},
        {"wine-prefix", required_argument, 0, 'w'},
        {"list",        no_argument,       0, 'L'},
        {"status",      required_argument, 0, 's'},
        {"verbose",     no_argument,       0, 'v'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "p:l:m:w:Ls:vh", longOptions, nullptr)) != -1) {
        switch (opt) {
            case 'p':
                // Check if it's a PID (number) or process name
                try {
                    targetPid = std::stoul(optarg);
                } catch (...) {
                    processName = optarg;
                }
                break;
                
            case 'l':
                libraryPath = optarg;
                break;
                
            case 'm':
                method = std::stoi(optarg);
                if (method < 1 || method > 4) {
                    fprintf(stderr, "Error: Invalid method. Must be 1-4.\n");
                    return 1;
                }
                break;
                
            case 'w':
                winePrefix = optarg;
                break;
                
            case 'L':
                listMode = true;
                break;
                
            case 's':
                statusMode = true;
                targetPid = std::stoul(optarg);
                break;
                
            case 'v':
                verbose = true;
                break;
                
            case 'h':
                PrintUsage(argv[0]);
                return 0;
                
            default:
                PrintUsage(argv[0]);
                return 1;
        }
    }
    
    // Handle list mode
    if (listMode) {
        if (processName.empty() && targetPid == 0) {
            fprintf(stderr, "Error: -p option required for list mode\n");
            return 1;
        }
        ListProcesses(processName.empty() ? std::to_string(targetPid) : processName);
        return 0;
    }
    
    // Handle status mode
    if (statusMode) {
        ShowProcessStatus(targetPid);
        return 0;
    }
    
    // Validate required arguments for injection
    if ((processName.empty() && targetPid == 0) || libraryPath.empty()) {
        fprintf(stderr, "Error: Both process (-p) and library (-l) are required for injection.\n");
        PrintUsage(argv[0]);
        return 1;
    }
    
    // Perform injection
    using namespace StealthInjector;
    
    StealthInjection injector;
    
    InjectionConfig config = {};
    config.targetProcess = processName;
    config.targetPid = targetPid;
    config.libraryPath = libraryPath;
    config.winePrefix = winePrefix;
    config.injectionMethod = method;
    config.verbose = verbose;
    config.antiDebug = true;
    config.hideModule = true;
    
    auto result = injector.Inject(config);
    
    printf("\n=== Injection Result ===\n");
    
    if (result.success) {
        printf("[+] SUCCESS!\n");
        printf("[+] Library injected at: 0x%lX\n", result.injectedBase);
        printf("[+] Target PID: %u\n", result.injectedPid);
    } else {
        printf("[-] FAILED: %s\n", result.error.c_str());
    }
    
    if (!result.warnings.empty()) {
        printf("\nWarnings:\n");
        for (const auto& warn : result.warnings) {
            printf("  [!] %s\n", warn.c_str());
        }
    }
    
    printf("\n");
    
    // If injection failed, suggest alternatives
    if (!result.success) {
        printf("=== Alternative Methods ===\n");
        printf("1. LD_PRELOAD method (requires game restart):\n");
        printf("   LD_PRELOAD=%s steam://rungameid/<gameid>\n", libraryPath.c_str());
        printf("\n");
        printf("2. For Proton games, add to Steam launch options:\n");
        printf("   LD_PRELOAD=$PWD/libbloodstrike.so %%command%%\n");
        printf("\n");
        printf("3. Use GDB for manual injection:\n");
        printf("   gdb -p <pid>\n");
        printf("   (gdb) call (void*)dlopen(\"/path/to/lib.so\", 1)\n");
    }
    
    return result.success ? 0 : 1;
}