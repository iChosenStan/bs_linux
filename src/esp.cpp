/**
 * ESP Implementation for BloodStrike
 */

#include "esp.h"
#include "entity_manager.h"
#include <cmath>
#include <cstring>
#include <iostream>

// ImGui includes for rendering
#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace ESP {

// ============================================================================
// SKELETON BONE CONNECTIONS
// ============================================================================
const std::vector<ESPRenderer::BoneConnection> ESPRenderer::skeletonConnections = {
    // Spine
    {GameOffsets::Bones::PELVIS, GameOffsets::Bones::SPINE},
    {GameOffsets::Bones::SPINE, GameOffsets::Bones::SPINE1},
    {GameOffsets::Bones::SPINE1, GameOffsets::Bones::SPINE2},
    {GameOffsets::Bones::SPINE2, GameOffsets::Bones::NECK},
    {GameOffsets::Bones::NECK, GameOffsets::Bones::HEAD},
    
    // Left arm
    {GameOffsets::Bones::NECK, GameOffsets::Bones::LEFT_CLAVICLE},
    {GameOffsets::Bones::LEFT_CLAVICLE, GameOffsets::Bones::LEFT_UPPER_ARM},
    {GameOffsets::Bones::LEFT_UPPER_ARM, GameOffsets::Bones::LEFT_FOREARM},
    {GameOffsets::Bones::LEFT_FOREARM, GameOffsets::Bones::LEFT_HAND},
    
    // Right arm
    {GameOffsets::Bones::NECK, GameOffsets::Bones::RIGHT_CLAVICLE},
    {GameOffsets::Bones::RIGHT_CLAVICLE, GameOffsets::Bones::RIGHT_UPPER_ARM},
    {GameOffsets::Bones::RIGHT_UPPER_ARM, GameOffsets::Bones::RIGHT_FOREARM},
    {GameOffsets::Bones::RIGHT_FOREARM, GameOffsets::Bones::RIGHT_HAND},
    
    // Left leg
    {GameOffsets::Bones::PELVIS, GameOffsets::Bones::LEFT_THIGH},
    {GameOffsets::Bones::LEFT_THIGH, GameOffsets::Bones::LEFT_CALF},
    {GameOffsets::Bones::LEFT_CALF, GameOffsets::Bones::LEFT_FOOT},
    
    // Right leg
    {GameOffsets::Bones::PELVIS, GameOffsets::Bones::RIGHT_THIGH},
    {GameOffsets::Bones::RIGHT_THIGH, GameOffsets::Bones::RIGHT_CALF},
    {GameOffsets::Bones::RIGHT_CALF, GameOffsets::Bones::RIGHT_FOOT}
};

// ============================================================================
// WEAPON NAMES
// ============================================================================
namespace {

const char* weaponNames[] = {
    "None",
    "Pistol",
    "Rifle",
    "Shotgun",
    "SMG",
    "Sniper",
    "LMG",
    "Melee",
    "Grenade",
    "Flashbang",
    "Smoke"
};

}

// ============================================================================
// ESP RENDERER IMPLEMENTATION
// ============================================================================
bool ESPRenderer::Initialize() {
    std::cout << "[ESP] Initialized" << std::endl;
    return true;
}

void ESPRenderer::Render() {
    if (!settings.enabled) return;
    
    auto& entityManager = EntitySystem::EntityManager::Get();
    auto localPlayer = entityManager.GetLocalPlayer();
    
    if (!localPlayer) return;
    
    // Get all entities
    const auto& entities = entityManager.GetEntities();
    
    // Render each entity
    for (const auto& entity : entities) {
        if (!ShouldRenderEntity(&entity)) continue;
        
        RenderEntity(const_cast<EntitySystem::Entity*>(&entity));
    }
}

void ESPRenderer::RenderEntity(EntitySystem::Entity* entity) {
    if (!entity || !entity->onScreen) return;
    
    // Get entity color
    float color[4];
    GetEntityColor(entity, color);
    
    // Render box
    if (settings.boxEnabled) {
        RenderBox(entity);
    }
    
    // Render skeleton
    if (settings.skeletonEnabled) {
        RenderSkeleton(entity);
    }
    
    // Render health bar
    if (settings.healthBarEnabled) {
        RenderHealthBar(entity);
    }
    
    // Render name
    if (settings.nameEnabled) {
        RenderName(entity);
    }
    
    // Render distance
    if (settings.distanceEnabled) {
        RenderDistance(entity);
    }
    
    // Render weapon
    if (settings.weaponEnabled) {
        RenderWeapon(entity);
    }
    
    // Render status
    if (settings.showStatus) {
        RenderStatus(entity);
    }
    
    // Render snapline
    if (settings.snaplineEnabled) {
        RenderSnapline(entity);
    }
    
    // Render head dot
    if (settings.headDotEnabled) {
        RenderHeadDot(entity);
    }
}

void ESPRenderer::RenderBox(EntitySystem::Entity* entity) {
    if (settings.box2D) {
        // 2D box - use 3D head position for calculation
        EntitySystem::Vector3 headPos = entity->GetBonePosition(GameOffsets::Bones::HEAD);
        auto box = Helpers::CalculateBox2D(entity->position, headPos, 
                                           screenWidth, screenHeight);
        if (!box.valid) return;
        
        float color[4];
        GetEntityColor(entity, color);
        
        if (settings.boxFill) {
            DrawFilledRect(box.x, box.y, box.w, box.h, 
                          settings.boxFillColor[0], settings.boxFillColor[1],
                          settings.boxFillColor[2], settings.boxFillColor[3]);
        }
        
        DrawRect(box.x, box.y, box.w, box.h, 
                color[0], color[1], color[2], color[3], settings.boxThickness);
    } else {
        // 3D box
        auto& viewMatrix = EntitySystem::EntityManager::Get().GetViewMatrix();
        auto box3d = Helpers::CalculateBox3D(entity->position, 0.5f, 1.8f, 
                                             viewMatrix, screenWidth, screenHeight);
        if (!box3d.valid) return;
        
        float color[4];
        GetEntityColor(entity, color);
        
        // Draw box edges
        for (int i = 0; i < 4; i++) {
            int next = (i + 1) % 4;
            DrawLine(box3d.corners[i].x, box3d.corners[i].y,
                    box3d.corners[next].x, box3d.corners[next].y,
                    color[0], color[1], color[2], color[3], settings.boxThickness);
            
            DrawLine(box3d.corners[i + 4].x, box3d.corners[i + 4].y,
                    box3d.corners[next + 4].x, box3d.corners[next + 4].y,
                    color[0], color[1], color[2], color[3], settings.boxThickness);
            
            DrawLine(box3d.corners[i].x, box3d.corners[i].y,
                    box3d.corners[i + 4].x, box3d.corners[i + 4].y,
                    color[0], color[1], color[2], color[3], settings.boxThickness);
        }
    }
}

void ESPRenderer::RenderSkeleton(EntitySystem::Entity* entity) {
    auto& viewMatrix = EntitySystem::EntityManager::Get().GetViewMatrix();
    
    for (const auto& conn : skeletonConnections) {
        EntitySystem::Vector3 bone1Pos = entity->GetBonePosition(
            static_cast<GameOffsets::Bones::BoneID>(conn.bone1));
        EntitySystem::Vector3 bone2Pos = entity->GetBonePosition(
            static_cast<GameOffsets::Bones::BoneID>(conn.bone2));
        
        EntitySystem::Vector2 bone1Screen, bone2Screen;
        
        if (viewMatrix.WorldToScreen(bone1Pos, bone1Screen, screenWidth, screenHeight) &&
            viewMatrix.WorldToScreen(bone2Pos, bone2Screen, screenWidth, screenHeight)) {
            
            DrawLine(bone1Screen.x, bone1Screen.y,
                    bone2Screen.x, bone2Screen.y,
                    settings.skeletonColor[0], settings.skeletonColor[1],
                    settings.skeletonColor[2], settings.skeletonColor[3],
                    settings.skeletonThickness);
        }
    }
}

void ESPRenderer::RenderHealthBar(EntitySystem::Entity* entity) {
    float healthPercent = entity->maxHealth > 0 ? entity->health / entity->maxHealth : 0.0f;
    healthPercent = std::max(0.0f, std::min(1.0f, healthPercent));
    
    EntitySystem::Vector3 headPos = entity->GetBonePosition(GameOffsets::Bones::HEAD);
    auto box = Helpers::CalculateBox2D(entity->position, headPos, 
                                       screenWidth, screenHeight);
    if (!box.valid) return;
    
    float barX, barY, barW, barH;
    
    if (settings.healthBarVertical) {
        barW = settings.healthBarWidth;
        barH = box.h;
        barX = box.x - barW - 5.0f;
        barY = box.y;
    } else {
        barW = box.w;
        barH = settings.healthBarWidth;
        barX = box.x;
        barY = box.y + box.h + 5.0f;
    }
    
    // Background
    DrawFilledRect(barX, barY, barW, barH,
                  settings.healthBarBgColor[0], settings.healthBarBgColor[1],
                  settings.healthBarBgColor[2], settings.healthBarBgColor[3]);
    
    // Health
    float healthColor[4];
    Helpers::GetHealthColor(healthPercent, healthColor);
    
    if (settings.healthBarVertical) {
        float healthH = barH * healthPercent;
        DrawFilledRect(barX, barY + (barH - healthH), barW, healthH,
                      healthColor[0], healthColor[1], healthColor[2], healthColor[3]);
    } else {
        float healthW = barW * healthPercent;
        DrawFilledRect(barX, barY, healthW, barH,
                      healthColor[0], healthColor[1], healthColor[2], healthColor[3]);
    }
    
    // Health text
    if (settings.showHealthText) {
        char healthText[32];
        snprintf(healthText, sizeof(healthText), "%.0f HP", entity->health);
        DrawText(barX + barW / 2, barY - 15.0f, healthText,
                healthColor[0], healthColor[1], healthColor[2], 1.0f,
                settings.nameSize, true);
    }
}

void ESPRenderer::RenderName(EntitySystem::Entity* entity) {
    EntitySystem::Vector3 headPos = entity->GetBonePosition(GameOffsets::Bones::HEAD);
    auto box = Helpers::CalculateBox2D(entity->position, headPos, 
                                       screenWidth, screenHeight);
    if (!box.valid) return;
    
    float y = box.y - 20.0f;
    
    DrawText(box.x + box.w / 2, y, entity->playerName,
            settings.nameColor[0], settings.nameColor[1],
            settings.nameColor[2], settings.nameColor[3],
            settings.nameSize, settings.nameShadow);
}

void ESPRenderer::RenderDistance(EntitySystem::Entity* entity) {
    EntitySystem::Vector3 headPos = entity->GetBonePosition(GameOffsets::Bones::HEAD);
    auto box = Helpers::CalculateBox2D(entity->position, headPos, 
                                       screenWidth, screenHeight);
    if (!box.valid) return;
    
    float y = box.y + box.h + 15.0f;
    
    char distText[32];
    snprintf(distText, sizeof(distText), "%.1fm", entity->distance);
    
    float color[4];
    Helpers::GetDistanceColor(entity->distance, settings.maxDistance, color);
    
    DrawText(box.x + box.w / 2, y, distText,
            color[0], color[1], color[2], color[3],
            settings.distanceSize, settings.distanceShadow);
}

void ESPRenderer::RenderWeapon(EntitySystem::Entity* entity) {
    EntitySystem::Vector3 headPos = entity->GetBonePosition(GameOffsets::Bones::HEAD);
    auto box = Helpers::CalculateBox2D(entity->position, headPos, 
                                       screenWidth, screenHeight);
    if (!box.valid) return;
    
    float y = box.y + box.h + 30.0f;
    
    const char* weaponName = GetWeaponName(entity->weaponId);
    
    DrawText(box.x + box.w / 2, y, weaponName,
            settings.weaponColor[0], settings.weaponColor[1],
            settings.weaponColor[2], settings.weaponColor[3],
            settings.weaponSize, settings.weaponShadow);
}

void ESPRenderer::RenderStatus(EntitySystem::Entity* entity) {
    EntitySystem::Vector3 headPos = entity->GetBonePosition(GameOffsets::Bones::HEAD);
    auto box = Helpers::CalculateBox2D(entity->position, headPos, 
                                       screenWidth, screenHeight);
    if (!box.valid) return;
    
    float x = box.x + box.w + 5.0f;
    float y = box.y;
    
    // Visibility indicator
    if (settings.showVisibility) {
        const char* status = entity->isVisible ? "V" : "H";
        float* color = entity->isVisible ? settings.visibleColor : settings.invisibleColor;
        
        DrawText(x, y, status, color[0], color[1], color[2], color[3],
                settings.statusSize, true);
        y += 12.0f;
    }
    
    // Crouch/Prone indicator
    if (entity->isCrouching) {
        DrawText(x, y, "C", 1.0f, 1.0f, 0.0f, 1.0f, settings.statusSize, true);
        y += 12.0f;
    }
    if (entity->isProne) {
        DrawText(x, y, "P", 1.0f, 0.5f, 0.0f, 1.0f, settings.statusSize, true);
        y += 12.0f;
    }
    
    // Reloading indicator
    if (entity->isReloading) {
        DrawText(x, y, "R", 1.0f, 0.0f, 1.0f, 1.0f, settings.statusSize, true);
    }
}

void ESPRenderer::RenderSnapline(EntitySystem::Entity* entity) {
    float startX, startY;
    
    if (settings.snaplineToCenter) {
        startX = screenWidth / 2.0f;
        startY = screenHeight / 2.0f;
    } else {
        startX = screenWidth / 2.0f;
        startY = screenHeight;
    }
    
    EntitySystem::Vector3 headPos = entity->GetBonePosition(GameOffsets::Bones::HEAD);
    auto box = Helpers::CalculateBox2D(entity->position, headPos, 
                                       screenWidth, screenHeight);
    if (!box.valid) return;
    
    float endX = box.x + box.w / 2.0f;
    float endY = box.y + box.h / 2.0f;
    
    DrawLine(startX, startY, endX, endY,
            settings.snaplineColor[0], settings.snaplineColor[1],
            settings.snaplineColor[2], settings.snaplineColor[3],
            settings.snaplineThickness);
}

void ESPRenderer::RenderHeadDot(EntitySystem::Entity* entity) {
    if (!entity->headOnScreen) return;
    
    DrawCircle(entity->headScreenPos.x, entity->headScreenPos.y,
              settings.headDotRadius,
              settings.headDotColor[0], settings.headDotColor[1],
              settings.headDotColor[2], settings.headDotColor[3],
              settings.headDotFill);
}

void ESPRenderer::DrawLine(float x1, float y1, float x2, float y2, 
                           float r, float g, float b, float a, float thickness) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (drawList) {
        drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), 
                          IM_COL32(r * 255, g * 255, b * 255, a * 255), 
                          thickness);
    }
}

void ESPRenderer::DrawRect(float x, float y, float w, float h, 
                           float r, float g, float b, float a, float thickness) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (drawList) {
        drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), 
                          IM_COL32(r * 255, g * 255, b * 255, a * 255), 
                          0.0f, 0, thickness);
    }
}

void ESPRenderer::DrawFilledRect(float x, float y, float w, float h, 
                                 float r, float g, float b, float a) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (drawList) {
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), 
                                IM_COL32(r * 255, g * 255, b * 255, a * 255));
    }
}

void ESPRenderer::DrawCircle(float x, float y, float radius, 
                             float r, float g, float b, float a, bool fill) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (drawList) {
        if (fill) {
            drawList->AddCircleFilled(ImVec2(x, y), radius,
                                      IM_COL32(r * 255, g * 255, b * 255, a * 255));
        } else {
            drawList->AddCircle(ImVec2(x, y), radius,
                                IM_COL32(r * 255, g * 255, b * 255, a * 255));
        }
    }
}

void ESPRenderer::DrawText(float x, float y, const char* text, 
                           float r, float g, float b, float a, float size, bool shadow) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (drawList) {
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImVec2 pos(x - textSize.x / 2, y);
        
        if (shadow) {
            drawList->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1, pos.y + 1),
                              IM_COL32(0, 0, 0, a * 255), text);
        }
        
        drawList->AddText(ImGui::GetFont(), size, pos,
                          IM_COL32(r * 255, g * 255, b * 255, a * 255), text);
    }
}

void ESPRenderer::GetEntityColor(const EntitySystem::Entity* entity, 
                                 float (&color)[4]) const {
    if (settings.useTeamColors) {
        auto localPlayer = EntitySystem::EntityManager::Get().GetLocalPlayer();
        if (localPlayer && entity->IsEnemy(localPlayer->team)) {
            memcpy(color, settings.enemyColor, sizeof(color));
        } else {
            memcpy(color, settings.friendlyColor, sizeof(color));
        }
    } else {
        memcpy(color, settings.boxColor, sizeof(color));
    }
}

bool ESPRenderer::ShouldRenderEntity(const EntitySystem::Entity* entity) const {
    if (!entity) return false;
    
    // Check alive
    if (settings.onlyAlive && !entity->isAlive) return false;
    
    // Check visibility
    if (settings.onlyVisible && !entity->isVisible) return false;
    
    // Check distance
    if (entity->distance < settings.minDistance || entity->distance > settings.maxDistance) return false;
    
    // Check team
    if (settings.onlyEnemies) {
        auto localPlayer = EntitySystem::EntityManager::Get().GetLocalPlayer();
        if (localPlayer && !entity->IsEnemy(localPlayer->team)) return false;
    }
    
    // Check on screen
    if (!entity->onScreen) return false;
    
    return true;
}

const char* ESPRenderer::GetWeaponName(int weaponId) const {
    if (weaponId >= 0 && weaponId < 11) {
        return weaponNames[weaponId];
    }
    return "Unknown";
}

// ============================================================================
// ESP HELPERS IMPLEMENTATION
// ============================================================================
namespace Helpers {

Box2D CalculateBox2D(const EntitySystem::Vector3& position, 
                     const EntitySystem::Vector3& headPosition,
                     int screenWidth, int screenHeight) {
    Box2D box;
    box.valid = false;
    
    // Calculate box height based on head and feet positions
    float height = abs(headPosition.y - position.y);
    float width = height * 0.5f;  // Aspect ratio
    
    box.x = headPosition.x - width / 2;
    box.y = headPosition.y;
    box.w = width;
    box.h = height;
    box.valid = true;
    
    return box;
}

Box3D CalculateBox3D(const EntitySystem::Vector3& position, 
                     float width, float height,
                     const EntitySystem::Matrix4x4& viewMatrix,
                     int screenWidth, int screenHeight) {
    Box3D box;
    box.valid = false;
    
    // Calculate 8 corners of the 3D box
    EntitySystem::Vector3 corners[8] = {
        {position.x - width, position.y, position.z - width},
        {position.x + width, position.y, position.z - width},
        {position.x + width, position.y, position.z + width},
        {position.x - width, position.y, position.z + width},
        {position.x - width, position.y + height, position.z - width},
        {position.x + width, position.y + height, position.z - width},
        {position.x + width, position.y + height, position.z + width},
        {position.x - width, position.y + height, position.z + width}
    };
    
    // Project all corners to screen space
    int validCorners = 0;
    for (int i = 0; i < 8; i++) {
        if (viewMatrix.WorldToScreen(corners[i], box.corners[i], screenWidth, screenHeight)) {
            validCorners++;
        }
    }
    
    box.valid = validCorners > 0;
    return box;
}

uint32_t ColorToRGBA(const float color[4]) {
    uint32_t r = static_cast<uint32_t>(color[0] * 255);
    uint32_t g = static_cast<uint32_t>(color[1] * 255);
    uint32_t b = static_cast<uint32_t>(color[2] * 255);
    uint32_t a = static_cast<uint32_t>(color[3] * 255);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

void GetHealthColor(float healthPercent, float (&color)[4]) {
    if (healthPercent > 0.5f) {
        // Green to yellow
        color[0] = 2.0f * (1.0f - healthPercent);
        color[1] = 1.0f;
        color[2] = 0.0f;
    } else {
        // Yellow to red
        color[0] = 1.0f;
        color[1] = 2.0f * healthPercent;
        color[2]  = 0.0f;
    }
    color[3] = 1.0f;
}

void GetDistanceColor(float distance, float maxDistance, float (&color)[4]) {
    float percent = distance / maxDistance;
    percent = std::max(0.0f, std::min(1.0f, percent));
    
    // Green (close) to red (far)
    color[0] = percent;
    color[1] = 1.0f - percent;
    color[2] = 0.0f;
    color[3] = 1.0f;
}

} // namespace Helpers

} // namespace ESP