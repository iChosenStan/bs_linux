/**
 * Blood Strike Linux Cheat
 * 
 * This is a Linux port of the Blood Strike cheat DLL.
 * 
 * EXTRACTED OFFSETS FROM WINDOWS DLL:
 *   Entry Point:       RVA 0x5BBD4
 *   Image Base:        0x180000000
 *   DirectX Present:   VTable offset 0x140
 *   Menu Toggle:       VK_INSERT (0x2D)
 *   
 * Global Variables (DLL Internal):
 *   g_Device:           RVA 0xA5EC8
 *   g_DeviceContext:    RVA 0xA5ED0
 *   g_SwapChain:        RVA 0xA5ED8
 *   g_OriginalPresent:  RVA 0xA5EB8
 *   g_MenuToggle:       RVA 0xA509E
 *   
 * Feature Strings:
 *   "Enabled##aimbot":  RVA 0x82770
 *   "ESP Active":       RVA 0x82840
 *   "2D Box ESP":       RVA 0x82880
 *   "Reduce Recoil":    RVA 0x82908
 *   "Reduce Spread":    RVA 0x82918
 *   
 * See offsets.h for complete offset definitions.
 * 
 * Features:
 *   - ImGui-based overlay menu
 *   - Vulkan rendering backend
 *   - ESP (Box, Skeleton, Health Bars)
 *   - Aimbot with FOV and smoothing
 *   - Weapon modifications
 *   - Process injection via ptrace/LD_PRELOAD
 * 
 * Build:
 *   mkdir build && cd build
 *   cmake .. && make
 * 
 * Usage:
 *   Method 1: LD_PRELOAD
 *     LD_PRELOAD=./libbloodstrike.so ./game
 * 
 *   Method 2: Injection
 *     ./injector <pid> ./libbloodstrike.so
 */

#include "cheat_features.h"
#include "menu.h"
#include "renderer.h"
#include "input.h"
#include "injector.h"
#include "hooks.h"
#include "offsets.h"  // Include extracted offsets
#include "game_offsets.h"  // Game-specific memory offsets
#include "entity_manager.h"  // Entity iteration system
#include "aimbot.h"  // Aimbot engine
#include "esp.h"  // ESP rendering
#include "pixelbot.h"  // Hardware pixelbot
#include "anticheat_bypass.h"  // Anti-cheat bypass

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>

// Global running flag
std::atomic<bool> g_running(true);

//=============================================================================
// Signal Handler
//=============================================================================
#include <signal.h>

void SignalHandler(int signal) {
    printf("\n[*] Received signal %d, shutting down...\n", signal);
    g_running = false;
}

//=============================================================================
// Constructor/Destructor (for LD_PRELOAD)
//=============================================================================
// Forward declaration
void MainLoop();

__attribute__((constructor))
void OnLoad() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           Blood Strike Linux Cheat v1.0.0                    ║\n");
    printf("║           Compiled for Fedora Linux                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Start cheat in separate thread
    std::thread([]() {
        MainLoop();
    }).detach();
}

__attribute__((destructor))
void OnUnload() {
    printf("[*] Unloading cheat...\n");
    g_running = false;
    Cheat::CheatManager::Instance().running = false;
    
    // Cleanup
    Hooks::Shutdown();
    Renderer::Shutdown();
    // Input handled by renderer
    
    printf("[+] Cheat unloaded successfully\n");
}

//=============================================================================
// Main Loop
//=============================================================================
void MainLoop() {
    printf("[*] Initializing cheat...\n");
    
    // Initialize subsystems
    if (!Renderer::Initialize()) {
        fprintf(stderr, "[-] Failed to initialize renderer\n");
        return;
    }
    printf("[+] Renderer initialized\n");
    
    Input::Initialize();
    printf("[+] Input initialized\n");
    
    Menu::Initialize();
    printf("[+] Menu initialized\n");
    
    Hooks::Initialize();
    printf("[+] Hooks initialized\n");
    
    // Initialize game systems
    EntitySystem::EntityManager::Get().Initialize();
    printf("[+] Entity Manager initialized\n");
    
    Aimbot::AimbotEngine::Get().Initialize();
    printf("[+] Aimbot Engine initialized\n");
    
    ESP::ESPRenderer::Get().Initialize();
    printf("[+] ESP Renderer initialized\n");
    
    // Pixelbot::PixelbotAimbot::Get().Initialize();  // Commented out - needs hardware setup
    printf("[+] Pixelbot available (hardware mode)\n");
    
    // Mark as initialized
    Cheat::CheatManager::Instance().initialized = true;
    
    printf("\n");
    printf("[+] Cheat loaded successfully!\n");
    printf("[+] Press INSERT to toggle menu\n");
    printf("[+] Press DELETE to unload\n");
    printf("\n");
    
    // Timing for delta time
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    // Main loop
    while (g_running && Cheat::CheatManager::Instance().running) {
        // Calculate delta time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Handle input
        Input::Update();
        
        // Check for unload key
        if (Input::IsKeyPressed(Input::Key::Delete)) {
            printf("[*] Unload requested\n");
            g_running = false;
            break;
        }
        
        // Handle menu input
        Menu::HandleInput();
        
        // Update game systems
        EntitySystem::EntityManager::Get().Update();
        
        // Update aimbot
        Aimbot::AimbotEngine::Get().Update(deltaTime);
        
        // Start new frame
        Renderer::NewFrame();
        
        // Render ESP
        if (Cheat::CheatManager::Instance().GetSettings().esp.active) {
            ESP::ESPRenderer::Get().Render();
            Cheat::ESP::Render();  // Legacy ESP
        }
        
        // Render aimbot FOV circle if enabled
        auto& aimbotSettings = Aimbot::AimbotEngine::Get().GetSettings();
        if (aimbotSettings.enabled && aimbotSettings.showFOV) {
            float fovX, fovY, fovRadius;
            Aimbot::AimbotEngine::Get().GetFOVCircle(fovX, fovY, fovRadius);
            // Draw FOV circle (would use ImGui draw list)
        }
        
        // Render menu
        Menu::Render();
        
        // Present
        Renderer::Present();
        
        // Small sleep to prevent high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    printf("[*] Main loop ended\n");
}

//=============================================================================
// Standalone Entry Point (for testing without injection)
//=============================================================================
#ifndef CHEAT_AS_LIBRARY
int main(int argc, char* argv[]) {
    // Install signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           Blood Strike Linux Cheat v1.0.0                    ║\n");
    printf("║           Standalone Mode (Testing)                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Check for injection mode
    if (argc >= 4 && strcmp(argv[1], "--inject") == 0) {
        uint32_t pid = std::stoul(argv[2]);
        const char* libPath = argv[3];
        
        printf("[*] Injection mode\n");
        printf("[*] Target PID: %u\n", pid);
        printf("[*] Library: %s\n", libPath);
        
        if (Injector::InjectLibrary(pid, libPath)) {
            printf("[+] Injection successful!\n");
            return 0;
        } else {
            fprintf(stderr, "[-] Injection failed\n");
            return 1;
        }
    }
    
    // Check for process finder mode
    if (argc >= 2 && strcmp(argv[1], "--find") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --find <process_name>\n", argv[0]);
            return 1;
        }
        
        auto proc = Injector::FindProcess(argv[2]);
        if (proc.pid != 0) {
            printf("Found process:\n");
            printf("  PID: %u\n", proc.pid);
            printf("  Name: %s\n", proc.name.c_str());
            printf("  Path: %s\n", proc.path.c_str());
            printf("  Base: 0x%lX\n", proc.baseAddress);
            return 0;
        } else {
            printf("Process not found: %s\n", argv[2]);
            return 1;
        }
    }
    
    // Run main loop
    MainLoop();
    
    // Cleanup
    Hooks::Shutdown();
    Renderer::Shutdown();
    // Input handled by renderer
    
    printf("[+] Goodbye!\n");
    return 0;
}
#endif