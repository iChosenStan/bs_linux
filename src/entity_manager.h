/**
 * Entity Manager for BloodStrike
 * Handles entity iteration, caching, and filtering
 */

#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <cmath>
#include <cstring>
#include "game_offsets.h"

namespace EntitySystem {

// ============================================================================
// VECTOR TYPES
// ============================================================================
struct Vector2 {
    float x, y;
    
    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}
    
    Vector2 operator+(const Vector2& other) const { return Vector2(x + other.x, y + other.y); }
    Vector2 operator-(const Vector2& other) const { return Vector2(x - other.x, y - other.y); }
    Vector2 operator*(float scalar) const { return Vector2(x * scalar, y * scalar); }
    
    float Length() const { return sqrtf(x * x + y * y); }
    float LengthSquared() const { return x * x + y * y; }
    
    Vector2 Normalize() const {
        float len = Length();
        if (len > 0) return Vector2(x / len, y / len);
        return *this;
    }
    
    float Dot(const Vector2& other) const { return x * other.x + y * other.y; }
};

struct Vector3 {
    float x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    Vector3 operator+(const Vector3& other) const { return Vector3(x + other.x, y + other.y, z + other.z); }
    Vector3 operator-(const Vector3& other) const { return Vector3(x - other.x, y - other.y, z - other.z); }
    Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }
    
    float Length() const { return sqrtf(x * x + y * y + z * z); }
    float LengthSquared() const { return x * x + y * y + z * z; }
    
    Vector3 Normalize() const {
        float len = Length();
        if (len > 0) return Vector3(x / len, y / len, z / len);
        return *this;
    }
    
    float Dot(const Vector3& other) const { return x * other.x + y * other.y + z * other.z; }
    Vector3 Cross(const Vector3& other) const {
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
    
    float Distance(const Vector3& other) const { return (*this - other).Length(); }
    
    // Calculate angle to target
    Vector2 CalcAngle(const Vector3& target) const {
        Vector3 delta = target - *this;
        float horizontalDist = sqrtf(delta.x * delta.x + delta.y * delta.y);
        
        Vector2 angles;
        angles.y = atan2f(delta.y, delta.x) * (180.0f / 3.14159265f);
        angles.x = -atan2f(delta.z, horizontalDist) * (180.0f / 3.14159265f);
        
        return angles;
    }
};

// ============================================================================
// MATRIX TYPES
// ============================================================================
struct Matrix4x4 {
    float m[4][4];
    
    Matrix4x4() {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }
    
    // World to screen transformation
    bool WorldToScreen(const Vector3& world, Vector2& screen, int screenW, int screenH) const {
        Vector3 screenPos;
        
        screenPos.x = world.x * m[0][0] + world.y * m[1][0] + world.z * m[2][0] + m[3][0];
        screenPos.y = world.x * m[0][1] + world.y * m[1][1] + world.z * m[2][1] + m[3][1];
        screenPos.z = world.x * m[0][2] + world.y * m[1][2] + world.z * m[2][2] + m[3][2];
        float w     = world.x * m[0][3] + world.y * m[1][3] + world.z * m[2][3] + m[3][3];
        
        if (w < 0.001f) return false;
        
        float invW = 1.0f / w;
        screenPos.x *= invW;
        screenPos.y *= invW;
        
        screen.x = (screenPos.x + 1.0f) * 0.5f * screenW;
        screen.y = (1.0f - screenPos.y) * 0.5f * screenH;
        
        return true;
    }
};

// ============================================================================
// ENTITY TYPES
// ============================================================================
enum class EntityType : int {
    INVALID     = 0,
    PLAYER      = 1,
    BOT         = 2,
    ITEM        = 3,
    PROJECTILE  = 4,
    VEHICLE     = 5,
    OBJECTIVE   = 6
};

enum class Team : int {
    NONE        = -1,
    SPECTATOR   = 0,
    TEAM_1      = 1,  // Friendly
    TEAM_2      = 2   // Enemy
};

// ============================================================================
// ENTITY CLASS
// ============================================================================
class Entity {
public:
    uint32_t entityId;
    EntityType entityType;
    Team team;
    
    // Status
    float health;
    float maxHealth;
    float armor;
    bool isAlive;
    bool isVisible;
    bool isCrouching;
    bool isProne;
    bool isReloading;
    
    // Position
    Vector3 position;
    Vector3 rotation;
    Vector3 velocity;
    Vector2 viewAngle;  // Yaw and Pitch only
    
    // Bone positions (for ESP skeleton)
    static const int MAX_BONES = 64;
    Vector3 bonePositions[MAX_BONES];
    
    // Player info
    char playerName[32];
    int playerLevel;
    int weaponId;
    
    // Calculated values
    float distance;
    Vector2 screenPos;
    Vector2 headScreenPos;
    bool onScreen;
    bool headOnScreen;
    
    // Memory address
    uintptr_t address;
    
    Entity() {
        memset(this, 0, sizeof(Entity));
        entityType = EntityType::INVALID;
        team = Team::NONE;
        isAlive = true;
    }
    
    // Check if entity is a valid target
    bool IsValidTarget() const {
        return isAlive && 
               health > 0 && 
               entityType == EntityType::PLAYER;
    }
    
    // Check if entity is enemy
    bool IsEnemy(Team localTeam) const {
        return team != localTeam && team != Team::NONE && team != Team::SPECTATOR;
    }
    
    // Get bone position by ID
    Vector3 GetBonePosition(int bone) const {
        if (bone >= 0 && bone < MAX_BONES) {
            return bonePositions[bone];
        }
        return Vector3();
    }
    
    // Update entity data from memory
    bool UpdateFromMemory(uintptr_t entityAddr);
};

// ============================================================================
// ENTITY FILTER
// ============================================================================
struct EntityFilter {
    bool includePlayers = true;
    bool includeBots = true;
    bool includeItems = false;
    bool includeEnemies = true;
    bool includeFriendlies = false;
    bool includeDead = false;
    bool includeInvisible = true;
    float maxDistance = 1000.0f;
    
    // Team filtering
    Team localTeam = Team::TEAM_1;
    
    bool PassesFilter(const Entity& entity) const {
        // Type filter
        if (!includePlayers && entity.entityType == EntityType::PLAYER) return false;
        if (!includeBots && entity.entityType == EntityType::BOT) return false;
        if (!includeItems && entity.entityType == EntityType::ITEM) return false;
        
        // Dead filter
        if (!includeDead && !entity.isAlive) return false;
        
        // Team filter
        if (!includeEnemies && entity.IsEnemy(localTeam)) return false;
        if (!includeFriendlies && !entity.IsEnemy(localTeam)) return false;
        
        // Visibility filter
        if (!includeInvisible && !entity.isVisible) return false;
        
        // Distance filter
        if (entity.distance > maxDistance) return false;
        
        return true;
    }
};

// ============================================================================
// ENTITY MANAGER
// ============================================================================
class EntityManager {
public:
    static EntityManager& Get() {
        static EntityManager instance;
        return instance;
    }
    
    // Initialize entity manager
    bool Initialize();
    
    // Update all entities (call every frame)
    void Update();
    
    // Get all entities
    const std::vector<Entity>& GetEntities() const { return entities; }
    
    // Get filtered entities
    std::vector<Entity*> GetFilteredEntities(const EntityFilter& filter);
    
    // Get local player
    Entity* GetLocalPlayer() { return localPlayerValid ? &localPlayer : nullptr; }
    
    // Get view matrix
    const Matrix4x4& GetViewMatrix() const { return viewMatrix; }
    
    // Get game state
    GameOffsets::GameManager::GameState GetGameState() const { return gameState; }
    
    // Find best target for aimbot
    Entity* FindBestTarget(const Vector3& localPos, const Vector2& viewAngles, 
                           float maxFOV, bool prioritizeVisible);
    
    // Get entity count
    size_t GetEntityCount() const { return entities.size(); }
    
    // Get active (alive) entity count
    size_t GetActiveEntityCount() const;
    
    // Check if initialized
    bool IsInitialized() const { return initialized; }
    
private:
    EntityManager() = default;
    
    // Entity storage
    std::vector<Entity> entities;
    Entity localPlayer;
    bool localPlayerValid = false;
    
    // View matrix
    Matrix4x4 viewMatrix;
    
    // Game state
    GameOffsets::GameManager::GameState gameState = GameOffsets::GameManager::GameState::MENU;
    
    // Threading
    std::mutex entityMutex;
    std::atomic<bool> initialized{false};
    std::atomic<bool> updating{false};
    
    // Memory addresses (set during initialization)
    uintptr_t gameManagerAddr = 0;
    uintptr_t entityListAddr = 0;
    uintptr_t localPlayerAddr = 0;
    uintptr_t viewMatrixAddr = 0;
    
    // Read entity from memory
    bool ReadEntity(uintptr_t address, Entity& entity);
    
    // Read local player
    bool ReadLocalPlayer();
    
    // Read view matrix
    bool ReadViewMatrix();
    
    // Update screen positions
    void UpdateScreenPositions(int screenW, int screenH);
};

} // namespace EntitySystem