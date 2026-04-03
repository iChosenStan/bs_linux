/**
 * Aimbot Implementation for BloodStrike
 */

#include "aimbot.h"
#include "entity_manager.h"
#include <cmath>
#include <random>
#include <iostream>
#include <cfloat>

namespace Aimbot {

// ============================================================================
// CONSTANTS
// ============================================================================
constexpr float PI = 3.14159265f;
constexpr float RAD2DEG = 180.0f / PI;
constexpr float DEG2RAD = PI / 180.0f;

// ============================================================================
// RANDOM NUMBER GENERATOR
// ============================================================================
static std::mt19937& GetRNG() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

static float RandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(GetRNG());
}

// ============================================================================
// RECOIL PATTERNS
// ============================================================================
namespace {

std::vector<RecoilPattern> recoilPatterns = {
    // Weapon ID 0 - Pistol
    {0, {{0, 2}, {0, 1.5f}, {0, 1}, {0, 0.5f}}, 0.3f},
    // Weapon ID 1 - Rifle (AK-47 style)
    {1, {{0, 2}, {0.5f, 3}, {1, 4}, {0, 5}, {-0.5f, 5}, {-1, 4}, {-1, 3}, {0, 2}}, 0.5f},
    // Weapon ID 2 - SMG
    {2, {{0, 1}, {0.2f, 1.5f}, {0.3f, 1.8f}, {0, 2}, {-0.2f, 2}}, 0.2f},
    // Weapon ID 3 - Shotgun
    {3, {{0, 5}}, 0.4f},
    // Weapon ID 4 - Sniper
    {4, {{0, 8}}, 0.6f},
    // Weapon ID 5 - LMG
    {5, {{0, 1.5f}, {0.3f, 2}, {0.5f, 2.5f}, {0.2f, 2.5f}, {0, 2}}, 0.15f}
};

}

const RecoilPattern& RecoilPattern::GetPattern(int weaponId) {
    static RecoilPattern defaultPattern = {0, {}, 0.3f};
    
    for (const auto& pattern : recoilPatterns) {
        if (pattern.weaponId == weaponId) {
            return pattern;
        }
    }
    return defaultPattern;
}

// ============================================================================
// HELPERS
// ============================================================================
namespace Helpers {

EntitySystem::Vector2 CalcAngle(const EntitySystem::Vector3& src, const EntitySystem::Vector3& dst) {
    EntitySystem::Vector3 delta = dst - src;
    float horizontalDist = sqrtf(delta.x * delta.x + delta.y * delta.y);
    
    EntitySystem::Vector2 angles;
    // Yaw = atan2(y, x) -> X is Forward, Y is Right (UE4)
    angles.y = atan2f(delta.y, delta.x) * RAD2DEG;
    // Pitch = atan2(z, horizontalDist) -> Z is Up (UE4)
    angles.x = -atan2f(delta.z, horizontalDist) * RAD2DEG;
    
    return angles;
}

float GetFovDifference(const EntitySystem::Vector2& viewAngles, 
                       const EntitySystem::Vector2& targetAngles) {
    EntitySystem::Vector2 delta = targetAngles - viewAngles;
    NormalizeAngle(delta);
    return sqrtf(delta.x * delta.x + delta.y * delta.y);
}

void NormalizeAngle(EntitySystem::Vector2& angle) {
    // Pitch clamp [-89.0, 89.0]
    if (angle.x > 89.0f) angle.x = 89.0f;
    if (angle.x < -89.0f) angle.x = -89.0f;
    
    // Yaw wrap [-180.0, 180.0]
    angle.y = fmodf(angle.y + 180.0f, 360.0f);
    if (angle.y < 0.0f) angle.y += 360.0f;
    angle.y -= 180.0f;
}

EntitySystem::Vector2 Lerp(const EntitySystem::Vector2& a, const EntitySystem::Vector2& b, float t) {
    return a + (b - a) * t;
}

EntitySystem::Vector2 SmoothStep(const EntitySystem::Vector2& a, 
                                  const EntitySystem::Vector2& b, 
                                  float t) {
    // Smoothstep interpolation for more natural movement
    t = t * t * (3.0f - 2.0f * t);
    return Lerp(a, b, t);
}

EntitySystem::Vector3 PredictPosition(const EntitySystem::Vector3& pos,
                                       const EntitySystem::Vector3& velocity,
                                       float bulletSpeed,
                                       float bulletGravity,
                                       float distance) {
    if (bulletSpeed <= 0.0f) return pos;
    
    // Calculate time for bullet to reach target
    float time = distance / bulletSpeed;
    
    // Predict position based on velocity
    EntitySystem::Vector3 predicted = pos + velocity * time;
    
    // Add bullet drop
    if (bulletGravity > 0.0f) {
        float drop = 0.5f * bulletGravity * time * time;
        predicted.y += drop;
    }
    
    return predicted;
}

bool IsInFOVCircle(float screenX, float screenY, float fovCenterX, float fovCenterY, float fovRadius) {
    float dx = screenX - fovCenterX;
    float dy = screenY - fovCenterY;
    return sqrtf(dx * dx + dy * dy) <= fovRadius;
}

} // namespace Helpers

// ============================================================================
// AIMBOT ENGINE IMPLEMENTATION
// ============================================================================
bool AimbotEngine::Initialize() {
    state.Reset();
    std::cout << "[Aimbot] Initialized" << std::endl;
    return true;
}

void AimbotEngine::Update(float deltaTime) {
    if (!settings.enabled) {
        state.Reset();
        return;
    }
    
    // Check if aim key is pressed (if required)
    if (settings.aimOnHotkey) {
        // TODO: Check if key is pressed
        // For now, we'll assume it's pressed
    }
    
    // Update timers
    state.reactionTimer -= deltaTime;
    state.targetSwitchTimer -= deltaTime;
    
    // Find or update target
    if (!state.currentTarget || !IsValidTarget(state.currentTarget)) {
        if (state.targetSwitchTimer <= 0.0f) {
            state.currentTarget = FindTarget();
            if (state.currentTarget) {
                state.targetSwitchTimer = settings.targetSwitchDelay;
            }
        }
    }
    
    // If we have a valid target
    if (state.currentTarget && IsValidTarget(state.currentTarget)) {
        // Calculate aim angle
        state.targetAngle = CalculateAimAngle(state.currentTarget);
        
        // Apply smoothing
        if (settings.smoothing) {
            state.smoothedAngle = SmoothAngle(state.currentAngle, state.targetAngle, deltaTime);
        } else {
            state.smoothedAngle = state.targetAngle;
        }
        
        // Apply humanization
        if (settings.humanization) {
            ApplyHumanization(state.smoothedAngle);
        }
        
        // Apply recoil control
        if (settings.recoilControl) {
            ApplyRecoilControl(state.smoothedAngle);
        }
        
        // Apply aim
        if (state.reactionTimer <= 0.0f) {
            // Check if silent aim
            if (settings.silentAim || settings.psilent) {
                // Write view angles directly to memory
                WriteViewAngles(state.smoothedAngle);
            } else {
                // Move mouse
                EntitySystem::Vector2 delta = state.smoothedAngle - state.currentAngle;
                Helpers::NormalizeAngle(delta);
                
                if (mouseCallback) {
                    mouseCallback(static_cast<int>(delta.x * 10), static_cast<int>(delta.y * 10));
                }
            }
            
            state.currentAngle = state.smoothedAngle;
            state.isAiming = true;
        }
        
        // Auto shoot
        if (settings.autoShoot && state.reactionTimer <= 0.0f) {
            float fovDiff = Helpers::GetFovDifference(state.currentAngle, state.targetAngle);
            if (fovDiff < 2.0f) {  // Within 2 degrees
                if (shootCallback) {
                    shootCallback();
                }
            }
        }
    } else {
        state.isAiming = false;
        state.currentTarget = nullptr;
    }
    
    state.lastTarget = state.currentTarget;
}

EntitySystem::Entity* AimbotEngine::FindTarget() {
    auto& entityManager = EntitySystem::EntityManager::Get();
    auto localPlayer = entityManager.GetLocalPlayer();
    
    if (!localPlayer) return nullptr;
    
    EntitySystem::Entity* bestTarget = nullptr;
    float bestScore = FLT_MAX;
    
    EntitySystem::EntityFilter filter;
    filter.includeEnemies = true;
    filter.includeFriendlies = false;
    filter.includeDead = false;
    filter.maxDistance = settings.maxDistance;
    filter.localTeam = localPlayer->team;
    
    auto entities = entityManager.GetFilteredEntities(filter);
    
    for (auto entity : entities) {
        if (!IsValidTarget(entity)) continue;
        
        // Get bone position
        EntitySystem::Vector3 targetPos = GetBestBonePosition(entity);
        
        // Calculate angle
        EntitySystem::Vector2 targetAngle = Helpers::CalcAngle(localPlayer->position, targetPos);
        
        // Get FOV difference
        float fovDiff = Helpers::GetFovDifference(localPlayer->viewAngle, targetAngle);
        
        // Check FOV
        if (fovDiff > settings.fov) continue;
        
        // Check distance
        float dist = localPlayer->position.Distance(entity->position);
        if (dist < settings.minDistance || dist > settings.maxDistance) continue;
        
        // Calculate score
        float score = fovDiff;
        score += (entity->distance / settings.maxDistance) * 10.0f;  // Distance penalty
        
        // Prefer visible targets
        if (settings.ignoreInvisible && !entity->isVisible) {
            score += 100.0f;
        } else if (entity->isVisible) {
            score -= 20.0f;
        }
        
        // Prefer lower health (easier to kill)
        if (entity->health < entity->maxHealth * 0.5f) {
            score -= 10.0f;
        }
        
        if (score < bestScore) {
            bestScore = score;
            bestTarget = entity;
        }
    }
    
    return bestTarget;
}

EntitySystem::Vector2 AimbotEngine::CalculateAimAngle(EntitySystem::Entity* target) {
    auto localPlayer = EntitySystem::EntityManager::Get().GetLocalPlayer();
    if (!localPlayer || !target) return {};
    
    // Get target position (best bone)
    EntitySystem::Vector3 targetPos = GetBestBonePosition(target);
    
    // Apply prediction
    if (settings.prediction) {
        ApplyPrediction(target, targetPos);
    }
    
    // Calculate angle
    return Helpers::CalcAngle(localPlayer->position, targetPos);
}

EntitySystem::Vector2 AimbotEngine::SmoothAngle(const EntitySystem::Vector2& current, 
                                                 const EntitySystem::Vector2& target,
                                                 float deltaTime) {
    EntitySystem::Vector2 delta = target - current;
    Helpers::NormalizeAngle(delta);
    
    float smoothFactor = settings.smoothAmount;
    
    // Add randomness if enabled
    if (settings.smoothRandom > 0.0f) {
        smoothFactor += RandomFloat(-settings.smoothRandom, settings.smoothRandom);
    }
    
    // Calculate smooth step
    float t = deltaTime * smoothFactor;
    t = std::min(t, 1.0f);
    
    return Helpers::SmoothStep(current, target, t);
}

void AimbotEngine::ApplyRecoilControl(EntitySystem::Vector2& angle) {
    if (!settings.recoilControl) return;
    
    // Get recoil pattern for current weapon
    // TODO: Get actual weapon ID from local player
    const auto& pattern = RecoilPattern::GetPattern(0);
    
    if (state.shotsFired < static_cast<int>(pattern.pattern.size())) {
        const auto& recoilKick = pattern.pattern[state.shotsFired];
        
        // Apply inverse recoil
        angle.x -= recoilKick.x * settings.recoilIntensity;
        angle.y -= recoilKick.y * settings.recoilIntensity;
    }
}

void AimbotEngine::ApplyPrediction(EntitySystem::Entity* target, EntitySystem::Vector3& predictedPos) {
    auto localPlayer = EntitySystem::EntityManager::Get().GetLocalPlayer();
    if (!localPlayer) return;
    
    float distance = localPlayer->position.Distance(target->position);
    
    predictedPos = Helpers::PredictPosition(
        target->position,
        target->velocity,
        settings.bulletSpeed,
        settings.bulletGravity,
        distance
    );
    
    // Add ping compensation
    if (settings.pingCompensation > 0.0f) {
        predictedPos = predictedPos + target->velocity * settings.pingCompensation;
    }
}

EntitySystem::Vector3 AimbotEngine::GetBestBonePosition(EntitySystem::Entity* target) {
    if (!target) return {};
    
    auto localPlayer = EntitySystem::EntityManager::Get().GetLocalPlayer();
    if (!localPlayer) return target->position;
    
    // Bone priority list
    static const int bonePriority[] = {
        GameOffsets::Bones::Head,
        GameOffsets::Bones::Neck,
        GameOffsets::Bones::Spine2,  // Upper spine/chest area
        GameOffsets::Bones::Pelvis
    };
    
    // If nearest bone mode, find closest to crosshair
    if (settings.nearestBone) {
        EntitySystem::Vector2 screenCenter = {1920.0f / 2.0f, 1080.0f / 2.0f};  // TODO: Get actual screen size
        EntitySystem::Vector2 bestScreenPos;
        float bestDist = FLT_MAX;
        EntitySystem::Vector3 bestPos = target->position;
        
        for (int bone : bonePriority) {
            EntitySystem::Vector3 bonePos = target->GetBonePosition(bone);
            EntitySystem::Vector2 screenPos;
            
            auto& viewMatrix = EntitySystem::EntityManager::Get().GetViewMatrix();
            if (viewMatrix.WorldToScreen(bonePos, screenPos, 1920, 1080)) {
                float dist = (screenPos - screenCenter).Length();
                if (dist < bestDist) {
                    bestDist = dist;
                    bestPos = bonePos;
                }
            }
        }
        return bestPos;
    }
    
    // Use configured target bone
    int boneIndex = settings.targetBone;
    if (boneIndex >= 0 && boneIndex < 4) {
        int boneId = bonePriority[boneIndex];
        return target->GetBonePosition(boneId);
    }
    
    return target->position;
}

void AimbotEngine::ApplyHumanization(EntitySystem::Vector2& angle) {
    // Add micro jitter
    if (settings.microJitter > 0.0f) {
        angle.x += RandomFloat(-settings.microJitter, settings.microJitter);
        angle.y += RandomFloat(-settings.microJitter, settings.microJitter);
    }
    
    // Simulate reaction time (only on new target)
    if (state.currentTarget != state.lastTarget && state.reactionTimer <= 0.0f) {
        state.reactionTimer = settings.reactionTime + RandomFloat(0.0f, 0.02f);
    }
}

bool AimbotEngine::IsValidTarget(EntitySystem::Entity* target) {
    if (!target) return false;
    
    if (settings.ignoreDead && !target->isAlive) return false;
    if (settings.ignoreFriends) {
        auto localPlayer = EntitySystem::EntityManager::Get().GetLocalPlayer();
        if (localPlayer && !target->IsEnemy(localPlayer->team)) return false;
    }
    
    if (target->distance < settings.minDistance || target->distance > settings.maxDistance) return false;
    
    return true;
}

bool AimbotEngine::IsInFOV(const EntitySystem::Vector2& angle) const {
    auto localPlayer = EntitySystem::EntityManager::Get().GetLocalPlayer();
    if (!localPlayer) return false;
    
    float fovDiff = Helpers::GetFovDifference(localPlayer->viewAngle, angle);
    return fovDiff <= settings.fov;
}

void AimbotEngine::ClampAngle(EntitySystem::Vector2& angle) {
    // Clamp pitch
    if (angle.x > 89.0f) angle.x = 89.0f;
    if (angle.x < -89.0f) angle.x = -89.0f;
    
    // Normalize yaw
    while (angle.y > 180.0f) angle.y -= 360.0f;
    while (angle.y < -180.0f) angle.y += 360.0f;
}

bool AimbotEngine::WriteViewAngles(const EntitySystem::Vector2& angle) {
    // TODO: Implement memory writing for silent aim
    // This would write to the game's view angles structure
    return false;
}

void AimbotEngine::SetTarget(EntitySystem::Entity* target) {
    if (target && IsValidTarget(target)) {
        state.currentTarget = target;
        state.targetSwitchTimer = 0.0f;
    }
}

void AimbotEngine::ClearTarget() {
    state.currentTarget = nullptr;
    state.isAiming = false;
}

void AimbotEngine::GetFOVCircle(float& x, float& y, float& radius) const {
    // Screen center
    x = 1920.0f / 2.0f;  // TODO: Get actual screen size
    y = 1080.0f / 2.0f;
    
    // Convert FOV degrees to pixels
    // This is an approximation
    radius = (settings.fov / 90.0f) * (1920.0f / 2.0f);
}

} // namespace Aimbot