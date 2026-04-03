#pragma once

//=============================================================================
// BLOOD STRIKE DLL - EXTRACTED OFFSETS & SIGNATURES
// Reverse Engineered from: bloodstrike.dll
// ImageBase: 0x180000000
// Entry Point: 0x5BBD4
//=============================================================================

#include <cstdint>

namespace Offsets {

//=============================================================================
// DLL INTERNAL OFFSETS (RVA - Relative Virtual Address)
// These are offsets within the cheat DLL itself, not game memory
//=============================================================================

namespace DLL {
    // Entry Point
    constexpr uintptr_t ENTRY_POINT = 0x5BBD4;

    // Global Variables (.data section)
    constexpr uintptr_t G_MODULE_HANDLE    = 0xA5EB0;  // HMODULE of injected DLL
    constexpr uintptr_t G_ORIGINAL_PRESENT = 0xA5EB8;  // Original IDXGISwapChain::Present pointer
    constexpr uintptr_t G_DEVICE_CONTEXT_PTR = 0xA5EC0; // ID3D11DeviceContext* storage
    constexpr uintptr_t G_DEVICE           = 0xA5EC8;  // ID3D11Device* pointer
    constexpr uintptr_t G_DEVICE_CONTEXT   = 0xA5ED0;  // ID3D11DeviceContext* pointer
    constexpr uintptr_t G_SWAP_CHAIN       = 0xA5ED8;  // IDXGISwapChain* pointer
    constexpr uintptr_t G_INITIALIZED      = 0xA5EE0;  // bool initialization flag
    constexpr uintptr_t G_PRESENT_CALLBACK = 0xA5EE8;  // Present hook callback pointer
    constexpr uintptr_t G_MENU_TOGGLE      = 0xA509E;  // Menu visibility toggle state
    constexpr uintptr_t G_STACK_COOKIE     = 0xA5300;  // Stack canary for security

    // Virtual Addresses (ImageBase + RVA)
    constexpr uintptr_t IMAGE_BASE = 0x180000000;
    
    constexpr uintptr_t VA_G_MODULE_HANDLE    = IMAGE_BASE + G_MODULE_HANDLE;
    constexpr uintptr_t VA_G_ORIGINAL_PRESENT = IMAGE_BASE + G_ORIGINAL_PRESENT;
    constexpr uintptr_t VA_G_DEVICE           = IMAGE_BASE + G_DEVICE;
    constexpr uintptr_t VA_G_DEVICE_CONTEXT   = IMAGE_BASE + G_DEVICE_CONTEXT;
    constexpr uintptr_t VA_G_SWAP_CHAIN       = IMAGE_BASE + G_SWAP_CHAIN;
    constexpr uintptr_t VA_G_INITIALIZED      = IMAGE_BASE + G_INITIALIZED;
    constexpr uintptr_t VA_G_MENU_TOGGLE      = IMAGE_BASE + G_MENU_TOGGLE;
}

//=============================================================================
// DIRECTX 11 VTABLE OFFSETS
// These are the VTable slot offsets for DirectX 11 interfaces
//=============================================================================

namespace DirectX {
    // IDXGISwapChain VTable
    constexpr uintptr_t VTABLE_QUERY_INTERFACE = 0x00;
    constexpr uintptr_t VTABLE_ADD_REF         = 0x08;
    constexpr uintptr_t VTABLE_RELEASE         = 0x10;
    constexpr uintptr_t VTABLE_PRESENT         = 0x140;  // *** HOOK TARGET ***
    constexpr uintptr_t VTABLE_GET_BUFFER      = 0x148;
    constexpr uintptr_t VTABLE_RESIZE_BUFFERS  = 0x138;

    // ID3D11Device VTable
    constexpr uintptr_t VTABLE_CREATE_VERTEX_SHADER = 0x38;
    constexpr uintptr_t VTABLE_CREATE_PIXEL_SHADER  = 0x48;
    constexpr uintptr_t VTABLE_CREATE_INPUT_LAYOUT  = 0x28;
    constexpr uintptr_t VTABLE_CREATE_BUFFER        = 0x18;

    // ID3D11DeviceContext VTable
    constexpr uintptr_t VTABLE_IA_SET_VERTEX_BUFFERS   = 0x60;
    constexpr uintptr_t VTABLE_OM_SET_RENDER_TARGETS   = 0x108;
    constexpr uintptr_t VTABLE_DRAW_INDEXED            = 0x10;
    constexpr uintptr_t VTABLE_DRAW                    = 0x18;
    constexpr uintptr_t VTABLE_MAP                     = 0x20;
    constexpr uintptr_t VTABLE_UNMAP                   = 0x28;
    constexpr uintptr_t VTABLE_PSSET_SHADER            = 0x30;
    constexpr uintptr_t VTABLE_VSSET_SHADER            = 0x38;
}

//=============================================================================
// BYTE PATTERNS & SIGNATURES
// For finding functions and data in memory
//=============================================================================

namespace Patterns {
    // Entry Point Pattern
    // Address: 0x18005BBD4
    const char ENTRY_POINT[] = "\x48\x83\xEC\x28\xE8\xBB\xFF\xFF\xFF\x48\xF7\xD8\x1B\xC0\xF7\xD8\xFF\xC8\x48\x83\xC4\x28\xC3";
    const char ENTRY_POINT_MASK[] = "xxxxxxxxxxxxxxxxxxxxxxx";

    // DirectX Present Hook Pattern
    // Address: 0x1800011D0
    const char PRESENT_HOOK[] = "\x40\x53\x55\x56\x48\x81\xEC\x90\x00\x00\x00\x48\x8B\x05";
    const char PRESENT_HOOK_MASK[] = "xxxxxxxxxxxxxx";

    // Menu Toggle Pattern (GetAsyncKeyState for VK_INSERT)
    // Address: 0x180001331
    const char MENU_TOGGLE[] = "\xB9\x2D\x00\x00\x00\xFF\x15";
    const char MENU_TOGGLE_MASK[] = "xx???xx";

    // ImGui Initialization Pattern
    // Address: 0x1800014C0
    const char IMGUI_INIT[] = "\x40\x55\x48\x8D\x6C\x24\xD0\x48\x81\xEC\x30\x01\x00\x00";
    const char IMGUI_INIT_MASK[] = "xxxxxxxxxxxxxx";

    // D3D11CreateDeviceAndSwapChain call pattern
    const char D3D11_CREATE[] = "\x48\x8B\x01\xFF\x90\x40\x01\x00\x00";
    const char D3D11_CREATE_MASK[] = "xxxxx????";
}

//=============================================================================
// FEATURE STRING OFFSETS (RVA in .rdata section)
// These are the menu item strings used to identify features
//=============================================================================

namespace Strings {
    // Menu Tab Names
    constexpr uintptr_t OVERLAY_UTILITY  = 0x82738;  // "Overlay Utility"
    constexpr uintptr_t MAIN_TABS        = 0x82748;  // "MainTabs"
    constexpr uintptr_t AIMBOT           = 0x82754;  // "Aimbot"
    constexpr uintptr_t CONFIGURATION    = 0x82760;  // "Configuration"
    constexpr uintptr_t WEAPON_MODS      = 0x827C0;  // "Weapon Mods"

    // Aimbot Feature Strings
    constexpr uintptr_t ENABLED_AIMBOT   = 0x82770;  // "Enabled##aimbot"
    constexpr uintptr_t TEAM_CHECK_AIMBOT = 0x82780; // "Team Check##aimbot"
    constexpr uintptr_t DRAW_FOV_CIRCLE  = 0x82798;  // "Draw FOV Circle"
    constexpr uintptr_t FOV_RADIUS       = 0x827C0;  // "FOV Radius"
    constexpr uintptr_t SMOOTH_VALUE     = 0x827D8;  // "Smooth Value"
    constexpr uintptr_t TARGET_BONE      = 0x82818;  // "Target Bone"
    constexpr uintptr_t FOV_DEGREE       = 0x82950;  // "FOV Degree"

    // ESP Feature Strings
    constexpr uintptr_t MAIN_TOGGLES     = 0x82830;  // "Main Toggles"
    constexpr uintptr_t ESP_ACTIVE       = 0x82840;  // "ESP Active"
    constexpr uintptr_t ENEMY_ONLY       = 0x82850;  // "Enemy Only (Team Check)"
    constexpr uintptr_t OVERLAY_ELEMENTS = 0x82868;  // "Overlay Elements"
    constexpr uintptr_t BOX_2D_ESP       = 0x82880;  // "2D Box ESP"
    constexpr uintptr_t SKELETON_ESP     = 0x82890;  // "Skeleton (Bone ESP)"
    constexpr uintptr_t HEALTH_BARS      = 0x828B8;  // "Health Bars"
    constexpr uintptr_t ESP_RANGE        = 0x828E0;  // "ESP Range"

    // Weapon Mod Strings
    constexpr uintptr_t REDUCE_RECOIL    = 0x82908;  // "Reduce Recoil"
    constexpr uintptr_t REDUCE_SPREAD    = 0x82918;  // "Reduce Spread"
    constexpr uintptr_t CUSTOM_FOV       = 0x82938;  // "Custom FOV"
    constexpr uintptr_t SETTINGS         = 0x827E8;  // "Settings"
}

//=============================================================================
// HOTKEY VALUES (Virtual Key Codes)
//=============================================================================

namespace Hotkeys {
    constexpr uint32_t VK_INSERT = 0x2D;   // Menu toggle
    constexpr uint32_t VK_DELETE = 0x2E;   // Panic key
    constexpr uint32_t VK_HOME   = 0x24;   // Feature toggle
    constexpr uint32_t VK_END    = 0x23;   // Feature toggle
    constexpr uint32_t VK_F1     = 0x70;   // Feature bind
    constexpr uint32_t VK_F2     = 0x71;   // Feature bind
    constexpr uint32_t VK_F3     = 0x72;   // Feature bind
    constexpr uint32_t VK_F4     = 0x73;   // Feature bind
}

//=============================================================================
// MEMORY SECTION RANGES (RVA)
//=============================================================================

namespace Sections {
    constexpr uintptr_t TEXT_START   = 0x1000;
    constexpr uintptr_t TEXT_END     = 0x81519;
    constexpr uintptr_t RDATA_START  = 0x82000;
    constexpr uintptr_t RDATA_END    = 0xA49DE;
    constexpr uintptr_t DATA_START   = 0xA5000;
    constexpr uintptr_t DATA_END     = 0xA5FFF;
    constexpr uintptr_t PDATA_START  = 0xA8000;
    constexpr uintptr_t PDATA_END    = 0xAE324;
}

//=============================================================================
// LINUX-SPECIFIC OFFSETS
// For Linux version using Vulkan/OpenGL
//=============================================================================

namespace Linux {
    // Vulkan function offsets (equivalent to DirectX Present)
    // These need to be found at runtime for the specific game/driver
    constexpr const char* VK_CREATE_SWAPCHAIN = "vkCreateSwapchainKHR";
    constexpr const char* VK_QUEUE_PRESENT = "vkQueuePresentKHR";
    constexpr const char* VK_ACQUIRE_NEXT_IMAGE = "vkAcquireNextImageKHR";

    // Wine/Proton compatibility
    // When running Windows games on Linux via Wine/Proton
    constexpr const char* WINE_D3D11_DLL = "wined3d11.dll";
    constexpr const char* DXVK_DLL = "dxvk_d3d11.dll";

    // Process memory access
    constexpr const char* PROC_MAPS = "/proc/%d/maps";
    constexpr const char* PROC_MEM = "/proc/%d/mem";
}

//=============================================================================
// GAME-SPECIFIC OFFSETS (PLACEHOLDERS)
// These would be found by scanning the actual game's memory
// They are NOT in the cheat DLL - they are external game offsets
//=============================================================================

namespace Game {
    // These are PLACEHOLDERS - real offsets need to be found via memory scanning
    // The Windows DLL doesn't contain these - they're external to the cheat

    struct Entity {
        // Example structure - real offsets need to be discovered
        uintptr_t position = 0x0;      // Entity position (X, Y, Z)
        uintptr_t health = 0x0;        // Entity health
        uintptr_t team = 0x0;          // Team ID
        uintptr_t boneMatrix = 0x0;    // Bone matrix for skeleton ESP
        uintptr_t isAlive = 0x0;       // Is entity alive?
        uintptr_t isValid = 0x0;       // Is entity valid?
    };

    struct LocalPlayer {
        uintptr_t base = 0x0;          // Local player base address
        uintptr_t viewAngles = 0x0;    // View angles (pitch, yaw)
        uintptr_t position = 0x0;      // Local player position
        uintptr_t team = 0x0;          // Local player team
    };

    struct GameManager {
        uintptr_t entityList = 0x0;    // Entity list pointer
        uintptr_t entityCount = 0x0;   // Number of entities
        uintptr_t localPlayer = 0x0;   // Local player pointer
    };

    // Weapon structure offsets
    struct Weapon {
        uintptr_t recoil = 0x0;        // Recoil value
        uintptr_t spread = 0x0;        // Spread value
        uintptr_t ammo = 0x0;          // Current ammo
        uintptr_t clip = 0x0;          // Clip size
    };

    // Note: These offsets must be found by:
    // 1. Memory scanning the game process
    // 2. Using a debugger to find variable addresses
    // 3. Analyzing the game's memory patterns
    // 4. Using existing offset databases for the specific game
}

} // namespace Offsets