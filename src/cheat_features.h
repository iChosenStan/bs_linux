#pragma once

#include <cstdint>
#include <atomic>
#include <mutex>
#include "offsets.h"  // Include extracted offsets from reverse engineering

namespace Cheat {

//=============================================================================
// Feature Settings Structure (Mirrors Windows DLL)
//=============================================================================
struct Settings {
    // Aimbot Settings
    struct {
        bool enabled = false;
        bool teamCheck = true;
        bool drawFovCircle = true;
        float fovRadius = 90.0f;
        float fovDegree = 180.0f;
        float smoothValue = 5.0f;
        int targetBone = 0;  // 0=head, 1=neck, 2=chest, 3=body
        float customFov = 90.0f;
    } aimbot;

    // ESP Settings  
    struct {
        bool active = false;
        bool enemyOnly = true;
        bool box2D = true;
        bool skeleton = true;
        bool healthBars = true;
        float range = 300.0f;
    } esp;

    // Weapon Modifications
    struct {
        bool reduceRecoil = false;
        bool reduceSpread = false;
        float customFov = 90.0f;
    } weaponMods;

    // Skin Changer Settings
    struct {
        bool enabled = false;
        bool forceAllWeapons = true;
        int selectedWeapon = 0;      // Index in weapon list
        int selectedSkin = 0;        // Index in skin list
        float wear = 0.0f;           // 0.0 = Factory New
        int seed = 0;                // Pattern seed
        bool enableStatTrak = false;
        int statTrakCount = 9999;
    } skinChanger;

    // Overlay Settings
    struct {
        bool showMenu = false;
        float opacity = 0.9f;
        int fontSize = 13;
    } overlay;
};

//=============================================================================
// Global State
//=============================================================================
class CheatManager {
public:
    static CheatManager& Instance() {
        static CheatManager instance;
        return instance;
    }

    Settings& GetSettings() { return m_settings; }
    std::mutex& GetMutex() { return m_mutex; }
    
    // State flags
    std::atomic<bool> initialized{false};
    std::atomic<bool> running{true};
    std::atomic<bool> attached{false};
    
    // Process info
    uint32_t targetPid = 0;
    void* processHandle = nullptr;
    void* moduleBase = nullptr;

private:
    CheatManager() = default;
    Settings m_settings;
    std::mutex m_mutex;
};

//=============================================================================
// Feature Implementations
//=============================================================================
namespace Aimbot {
    void Update();
    void SetTargetBone(int bone);
    float CalculateFov(const float* viewAngles, const float* targetAngles);
    void SmoothAim(float* currentAngles, const float* targetAngles, float smoothAmount);
}

namespace ESP {
    void Render();
    void DrawBox2D(float x, float y, float w, float h, uint32_t color);
    void DrawSkeleton(const float* bonePositions, int boneCount);
    void DrawHealthBar(float x, float y, float health, float maxHealth);
    void DrawFovCircle(float radius);
}

namespace WeaponMods {
    void ApplyNoRecoil();
    void ApplyNoSpread();
    void SetFov(float fov);
}

namespace Memory {
    bool Read(uintptr_t address, void* buffer, size_t size);
    bool Write(uintptr_t address, const void* buffer, size_t size);
    uintptr_t FindPattern(const char* pattern, const char* mask, uintptr_t start, size_t range);
}

} // namespace Cheat