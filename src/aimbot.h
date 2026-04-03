/**
 * Aimbot System for BloodStrike
 * Features: Silent aim, memory aim, pixel aim, smoothing, prediction
 */

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include "entity_manager.h"
#include "game_offsets.h"

namespace Aimbot {

// ============================================================================
// AIMBOT SETTINGS
// ============================================================================
struct AimbotSettings {
    // General
    bool enabled = false;
    bool silentAim = false;          // Don't move view (server-side only)
    bool autoShoot = false;          // Automatically shoot when on target
    bool requireTarget = true;       // Only shoot when valid target
    
    // Targeting
    bool targetPlayers = true;
    bool targetBots = true;
    bool ignoreFriends = true;
    bool ignoreDead = true;
    bool ignoreInvisible = false;    // Target through walls
    
    // FOV
    float fov = 90.0f;               // Field of view in degrees
    bool showFOV = true;             // Draw FOV circle on screen
    float fovColor[4] = {1.0f, 1.0f, 1.0f, 0.5f};
    
    // Smoothing
    bool smoothing = true;
    float smoothAmount = 5.0f;       // 1 = instant, higher = slower
    float smoothRandom = 0.0f;       // Add randomness to appear human
    
    // Bone targeting
    int targetBone = 0;              // 0 = head, 1 = neck, 2 = chest, 3 = pelvis
    bool nearestBone = false;        // Target closest bone to crosshair
    bool scanBones = true;           // If primary bone not visible, try others
    
    // Prediction
    bool prediction = true;
    float bulletSpeed = 900.0f;      // Weapon bullet speed
    float bulletGravity = 0.0f;      // Bullet drop (0 for hitscan)
    float pingCompensation = 0.0f;   // Add extra prediction for lag
    
    // Recoil control
    bool recoilControl = false;
    float recoilIntensity = 1.0f;    // 0-1, how much to counter
    int recoilPatternId = 0;         // Weapon-specific pattern
    
    // Spread control
    bool spreadControl = false;
    float spreadReduction = 0.5f;    // 0-1, reduce spread by %
    
    // Hotkeys
    int aimKey = 0x02;               // Right mouse button (VK_RBUTTON)
    bool aimOnHotkey = true;         // Require hotkey to aim
    bool toggleMode = false;         // Toggle instead of hold
    
    // Distance limits
    float minDistance = 0.0f;
    float maxDistance = 500.0f;
    
    // Humanization
    bool humanization = true;
    float reactionTime = 0.05f;      // Simulated reaction delay
    float microJitter = 0.1f;        // Add small random movements
    float targetSwitchDelay = 0.2f;  // Delay before switching targets
    
    // Advanced
    bool psilent = false;            // Perfect silent aim (pSilent)
    bool backtrack = false;          // Shoot at where enemy was
    float backtrackTime = 0.2f;      // How far back to aim (seconds)
    
    // Hardware mode (for pixelbot)
    bool hardwareMode = false;
    int hardwarePort = 0;            // Serial port for Arduino
};

// ============================================================================
// RECOIL PATTERNS
// ============================================================================
struct RecoilPattern {
    int weaponId;
    std::vector<EntitySystem::Vector2> pattern;  // Recoil kicks per shot
    float recoveryTime;                           // Time to return to center
    
    static const RecoilPattern& GetPattern(int weaponId);
};

// ============================================================================
// AIMBOT STATE
// ============================================================================
struct AimbotState {
    EntitySystem::Entity* currentTarget = nullptr;
    EntitySystem::Entity* lastTarget = nullptr;
    
    float targetSwitchTimer = 0.0f;
    float reactionTimer = 0.0f;
    
    int shotsFired = 0;
    float lastShotTime = 0.0f;
    
    EntitySystem::Vector2 currentAngle;
    EntitySystem::Vector2 targetAngle;
    EntitySystem::Vector2 smoothedAngle;
    
    bool isAiming = false;
    bool targetLocked = false;
    
    void Reset() {
        currentTarget = nullptr;
        targetSwitchTimer = 0.0f;
        reactionTimer = 0.0f;
        shotsFired = 0;
        lastShotTime = 0.0f;
        isAiming = false;
        targetLocked = false;
    }
};

// ============================================================================
// AIMBOT ENGINE
// ============================================================================
class AimbotEngine {
public:
    static AimbotEngine& Get() {
        static AimbotEngine instance;
        return instance;
    }
    
    // Initialize aimbot
    bool Initialize();
    
    // Update aimbot (call every frame)
    void Update(float deltaTime);
    
    // Get current settings
    AimbotSettings& GetSettings() { return settings; }
    const AimbotSettings& GetSettings() const { return settings; }
    
    // Get current state
    const AimbotState& GetState() const { return state; }
    
    // Set callbacks
    void SetMouseCallback(std::function<void(int, int)> callback) { mouseCallback = callback; }
    void SetShootCallback(std::function<void()> callback) { shootCallback = callback; }
    
    // Manual target selection
    void SetTarget(EntitySystem::Entity* target);
    void ClearTarget();
    
    // Check if aimbot is active
    bool IsActive() const { return state.isAiming && state.currentTarget != nullptr; }
    
    // Get FOV circle data for rendering
    void GetFOVCircle(float& x, float& y, float& radius) const;
    
private:
    AimbotEngine() = default;
    
    // Settings
    AimbotSettings settings;
    AimbotState state;
    
    // Callbacks
    std::function<void(int, int)> mouseCallback;
    std::function<void()> shootCallback;
    
    // Internal methods
    EntitySystem::Entity* FindTarget();
    EntitySystem::Vector2 CalculateAimAngle(EntitySystem::Entity* target);
    EntitySystem::Vector2 SmoothAngle(const EntitySystem::Vector2& current, 
                                       const EntitySystem::Vector2& target,
                                       float deltaTime);
    void ApplyRecoilControl(EntitySystem::Vector2& angle);
    void ApplyPrediction(EntitySystem::Entity* target, EntitySystem::Vector3& predictedPos);
    
    // Bone selection
    EntitySystem::Vector3 GetBestBonePosition(EntitySystem::Entity* target);
    
    // Humanization
    void ApplyHumanization(EntitySystem::Vector2& angle);
    
    // Check if target is valid
    bool IsValidTarget(EntitySystem::Entity* target);
    
    // FOV check
    bool IsInFOV(const EntitySystem::Vector2& angle) const;
    
    // Angle clamping
    void ClampAngle(EntitySystem::Vector2& angle);
    
    // Memory writing (for silent aim)
    bool WriteViewAngles(const EntitySystem::Vector2& angle);
};

// ============================================================================
// AIMBOT HELPERS
// ============================================================================
namespace Helpers {

// Calculate angle from source to target
EntitySystem::Vector2 CalcAngle(const EntitySystem::Vector3& src, const EntitySystem::Vector3& dst);

// Calculate angle difference (FOV)
float GetFovDifference(const EntitySystem::Vector2& viewAngles, 
                       const EntitySystem::Vector2& targetAngles);

// Normalize angle to -180 to 180 range
void NormalizeAngle(EntitySystem::Vector2& angle);

// Linear interpolation
EntitySystem::Vector2 Lerp(const EntitySystem::Vector2& a, const EntitySystem::Vector2& b, float t);

// Smooth step
EntitySystem::Vector2 SmoothStep(const EntitySystem::Vector2& a, 
                                  const EntitySystem::Vector2& b, 
                                  float t);

// Calculate bullet drop compensation
EntitySystem::Vector3 PredictPosition(const EntitySystem::Vector3& pos,
                                       const EntitySystem::Vector3& velocity,
                                       float bulletSpeed,
                                       float bulletGravity,
                                       float distance);

// Check if point is inside screen FOV circle
bool IsInFOVCircle(float screenX, float screenY, float fovCenterX, float fovCenterY, float fovRadius);

} // namespace Helpers

} // namespace Aimbot