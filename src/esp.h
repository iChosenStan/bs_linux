/**
 * ESP (Extra Sensory Perception) Rendering System
 * Features: Box ESP, Skeleton ESP, Health bars, Distance, Name, Weapon info
 */

#pragma once

#include <vector>
#include <string>
#include "entity_manager.h"
#include "game_offsets.h"

namespace ESP {

// ============================================================================
// ESP SETTINGS
// ============================================================================
struct ESPSettings {
    // General
    bool enabled = false;
    bool onlyEnemies = true;
    bool onlyVisible = false;
    bool onlyAlive = true;
    
    // Distance limits
    float minDistance = 0.0f;
    float maxDistance = 500.0f;
    
    // Box ESP
    bool boxEnabled = true;
    bool box2D = true;              // 2D box vs 3D box
    float boxColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};  // RGBA
    float boxThickness = 1.0f;
    bool boxFill = false;
    float boxFillColor[4] = {1.0f, 0.0f, 0.0f, 0.1f};
    
    // Skeleton ESP
    bool skeletonEnabled = true;
    float skeletonColor[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float skeletonThickness = 1.0f;
    
    // Health bar
    bool healthBarEnabled = true;
    bool healthBarVertical = true;  // Vertical vs horizontal
    float healthBarWidth = 3.0f;
    float healthBarHeight = 50.0f;
    float healthBarColor[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float healthBarBgColor[4] = {0.0f, 0.0f, 0.0f, 0.5f};
    bool showHealthText = true;
    
    // Name ESP
    bool nameEnabled = true;
    float nameColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float nameSize = 14.0f;
    bool nameShadow = true;
    
    // Distance ESP
    bool distanceEnabled = true;
    float distanceColor[4] = {1.0f, 1.0f, 0.0f, 1.0f};
    float distanceSize = 12.0f;
    bool distanceShadow = true;
    
    // Weapon ESP
    bool weaponEnabled = true;
    float weaponColor[4] = {0.0f, 1.0f, 1.0f, 1.0f};
    float weaponSize = 12.0f;
    bool weaponShadow = true;
    
    // Status indicators
    bool showStatus = true;
    float statusSize = 10.0f;
    
    // Team colors
    bool useTeamColors = true;
    float enemyColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float friendlyColor[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    
    // Visibility indicator
    bool showVisibility = true;
    float visibleColor[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float invisibleColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    
    // Snaplines
    bool snaplineEnabled = false;
    float snaplineColor[4] = {1.0f, 1.0f, 1.0f, 0.5f};
    float snaplineThickness = 1.0f;
    bool snaplineToCenter = true;  // To screen center vs bottom
    
    // Head dot
    bool headDotEnabled = true;
    float headDotRadius = 3.0f;
    float headDotColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    bool headDotFill = true;
    
    // Tracers (bullet trajectory)
    bool tracersEnabled = false;
    float tracerColor[4] = {1.0f, 1.0f, 0.0f, 0.5f};
    float tracerThickness = 1.0f;
    
    // Item ESP
    bool itemsEnabled = false;
    float itemColor[4] = {1.0f, 0.5f, 0.0f, 1.0f};
    float itemSize = 12.0f;
    bool itemBox = true;
    
    // Vehicle ESP
    bool vehiclesEnabled = false;
    float vehicleColor[4] = {0.5f, 0.5f, 1.0f, 1.0f};
    float vehicleSize = 12.0f;
    bool vehicleBox = true;
};

// ============================================================================
// ESP RENDERER
// ============================================================================
class ESPRenderer {
public:
    static ESPRenderer& Get() {
        static ESPRenderer instance;
        return instance;
    }
    
    // Initialize renderer
    bool Initialize();
    
    // Render ESP (call every frame)
    void Render();
    
    // Get settings
    ESPSettings& GetSettings() { return settings; }
    const ESPSettings& GetSettings() const { return settings; }
    
    // Set screen dimensions
    void SetScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
    }
    
private:
    ESPRenderer() = default;
    
    // Settings
    ESPSettings settings;
    
    // Screen dimensions
    int screenWidth = 1920;
    int screenHeight = 1080;
    
    // Rendering methods
    void RenderEntity(EntitySystem::Entity* entity);
    void RenderBox(EntitySystem::Entity* entity);
    void RenderSkeleton(EntitySystem::Entity* entity);
    void RenderHealthBar(EntitySystem::Entity* entity);
    void RenderName(EntitySystem::Entity* entity);
    void RenderDistance(EntitySystem::Entity* entity);
    void RenderWeapon(EntitySystem::Entity* entity);
    void RenderStatus(EntitySystem::Entity* entity);
    void RenderSnapline(EntitySystem::Entity* entity);
    void RenderHeadDot(EntitySystem::Entity* entity);
    
    // Helper methods
    void DrawLine(float x1, float y1, float x2, float y2, 
                  float r, float g, float b, float a, float thickness);
    void DrawRect(float x, float y, float w, float h, 
                  float r, float g, float b, float a, float thickness);
    void DrawFilledRect(float x, float y, float w, float h, 
                        float r, float g, float b, float a);
    void DrawCircle(float x, float y, float radius, 
                    float r, float g, float b, float a, bool fill);
    void DrawText(float x, float y, const char* text, 
                  float r, float g, float b, float a, float size, bool shadow);
    
    // Color helpers
    void GetEntityColor(const EntitySystem::Entity* entity, 
                        float (&color)[4]) const;
    
    // Bone connections for skeleton
    struct BoneConnection {
        int bone1;
        int bone2;
    };
    static const std::vector<BoneConnection> skeletonConnections;
    
    // Check if entity should be rendered
    bool ShouldRenderEntity(const EntitySystem::Entity* entity) const;
    
    // Get weapon name from ID
    const char* GetWeaponName(int weaponId) const;
};

// ============================================================================
// ESP HELPERS
// ============================================================================
namespace Helpers {

// Calculate 2D box from 3D position
struct Box2D {
    float x, y, w, h;
    bool valid;
};

Box2D CalculateBox2D(const EntitySystem::Vector3& position, 
                     const EntitySystem::Vector3& headPosition,
                     int screenWidth, int screenHeight);

// Calculate box corners for 3D box
struct Box3D {
    EntitySystem::Vector2 corners[8];
    bool valid;
};

Box3D CalculateBox3D(const EntitySystem::Vector3& position, 
                     float width, float height,
                     const EntitySystem::Matrix4x4& viewMatrix,
                     int screenWidth, int screenHeight);

// Convert color from float[4] to uint32_t
uint32_t ColorToRGBA(const float color[4]);

// Get health color based on percentage
void GetHealthColor(float healthPercent, float (&color)[4]);

// Get distance color based on distance
void GetDistanceColor(float distance, float maxDistance, float (&color)[4]);

} // namespace Helpers

} // namespace ESP