#include "menu.h"
#include "renderer.h"
#include "input.h"
#include "skin_changer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace Menu {

static bool s_showMenu = false;
static int s_targetBoneIndex = 0;
static const char* s_boneNames[] = { "Head", "Neck", "Chest", "Body" };

// Skin changer arrays
static int s_selectedWeapon = 0;
static int s_selectedSkin = 0;
static float s_wearValue = 0.0f;
static int s_seedValue = 0;
static bool s_statTrakEnabled = false;
static int s_statTrakCount = 9999;

// Build weapon/skin name arrays for combo boxes
static const char* s_weaponNames[] = {
    "AK-47", "M4A4", "M4A1-S", "AWP", "Desert Eagle", "Glock-18", "USP-S",
    "MP7", "MP9", "MAC-10", "UMP-45", "P90", "FAMAS", "Galil AR",
    "AUG", "SG 553", "SCAR-20", "SSG 08", "Nova", "XM1014"
};

static const char* s_skinNames[] = {
    // AK-47 Skins
    "AK-47 | Dragon Lore", "AK-47 | Neon Rider", "AK-47 | Asiimov",
    "AK-47 | Fire Serpent", "AK-47 | Vulcan", "AK-47 | Bloodsport",
    "AK-47 | Phantom Disruptor", "AK-47 | Wilderlotus",
    // M4A4 Skins
    "M4A4 | Howl", "M4A4 | Asiimov", "M4A4 | Desolate Space",
    "M4A4 | The Emperor", "M4A4 | Bloodshot",
    // M4A1-S Skins
    "M4A1-S | Printstream", "M4A1-S | Bright Water", "M4A1-S | Hyper Beast",
    // AWP Skins
    "AWP | Dragon Lore", "AWP | Gungnir", "AWP | The Prince",
    "AWP | Asiimov", "AWP | Hyper Beast", "AWP | Fade", "AWP | Doppler",
    // Pistol Skins
    "Desert Eagle | Blaze", "Desert Eagle | Printstream", "Desert Eagle | Code Red",
    "Glock-18 | Fade", "Glock-18 | Water Elemental",
    "USP-S | Kill Confirmed", "USP-S | Printstream", "USP-S | Orion",
    // SMG Skins
    "MP7 | Akuli", "MP9 | Starlight Protector",
    "P90 | Asiimov", "UMP-45 | Motorpool",
    // Knife Skins
    "Karambit | Doppler", "Karambit | Fade", "Karambit | Marble Fade",
    "Butterfly | Doppler", "Butterfly | Fade",
    "M9 Bayonet | Doppler", "M9 Bayonet | Fade"
};

static const char* s_wearNames[] = {
    "Factory New", "Minimal Wear", "Field-Tested", "Well-Worn", "Battle-Scarred"
};
static float s_wearValues[] = { 0.0f, 0.07f, 0.15f, 0.38f, 0.45f };

void Initialize() {
    ImGui::CreateContext();
    
    // Configure ImGui
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Set font
    io.Fonts->AddFontDefault();
    
    ApplyStyle();
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Premium Gaming Theme (Blood Red Accent)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.07f, 0.07f, 0.09f, 0.96f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.30f, 0.30f, 0.36f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.91f, 0.27f, 0.38f, 1.00f); // Blood Red
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.91f, 0.27f, 0.38f, 0.70f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.91f, 0.27f, 0.38f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.91f, 0.27f, 0.38f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.91f, 0.27f, 0.38f, 0.80f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.91f, 0.27f, 0.38f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.91f, 0.27f, 0.38f, 0.30f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.91f, 0.27f, 0.38f, 0.70f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.91f, 0.27f, 0.38f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.30f, 0.30f, 0.36f, 0.50f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.91f, 0.27f, 0.38f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.91f, 0.27f, 0.38f, 1.00f);
    
    style.WindowRounding = 12.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 8.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(5, 5);
    style.ItemSpacing = ImVec2(10, 8);
}

void Render() {
    if (!s_showMenu) return;
    
    auto& settings = Cheat::CheatManager::Instance().GetSettings();
    
    // Set window position and size
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
    
    // Begin main window
    ImGui::Begin("Overlay Utility", &s_showMenu, 
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    // Tab bar
    if (ImGui::BeginTabBar("MainTabs")) {
        
        //========== OVERLAY UTILITY TAB ==========
        if (ImGui::BeginTabItem("Overlay Utility")) {
            
            ImGui::Text("Main Toggles");
            ImGui::Separator();
            
            ImGui::Checkbox("ESP Active", &settings.esp.active);
            ImGui::Checkbox("Enemy Only (Team Check)", &settings.esp.enemyOnly);
            
            ImGui::Spacing();
            ImGui::Text("Overlay Elements");
            ImGui::Separator();
            
            ImGui::Checkbox("2D Box ESP", &settings.esp.box2D);
            ImGui::Checkbox("Skeleton (Bone ESP)", &settings.esp.skeleton);
            ImGui::Checkbox("Health Bars", &settings.esp.healthBars);
            
            ImGui::Spacing();
            ImGui::SliderFloat("ESP Range", &settings.esp.range, 0.0f, 500.0f, "%.1f");
            
            ImGui::EndTabItem();
        }
        
        //========== AIMBOT TAB ==========
        if (ImGui::BeginTabItem("Aimbot")) {
            
            ImGui::Checkbox("Enabled", &settings.aimbot.enabled);
            ImGui::Checkbox("Team Check", &settings.aimbot.teamCheck);
            ImGui::Checkbox("Draw FOV Circle", &settings.aimbot.drawFovCircle);
            
            ImGui::Spacing();
            ImGui::Text("Aimbot Settings");
            ImGui::Separator();
            
            ImGui::SliderFloat("FOV Radius", &settings.aimbot.fovRadius, 0.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("FOV Degree", &settings.aimbot.fovDegree, 0.0f, 360.0f, "%.1f");
            ImGui::SliderFloat("Smooth Value", &settings.aimbot.smoothValue, 1.0f, 20.0f, "%.1f");
            
            ImGui::Spacing();
            ImGui::Combo("Target Bone", &s_targetBoneIndex, s_boneNames, IM_ARRAYSIZE(s_boneNames));
            settings.aimbot.targetBone = s_targetBoneIndex;
            
            ImGui::EndTabItem();
        }
        
        //========== WEAPON MODS TAB ==========
        if (ImGui::BeginTabItem("Weapon Mods")) {
            
            ImGui::Text("Weapon Modifications");
            ImGui::Separator();
            
            ImGui::Checkbox("Reduce Recoil", &settings.weaponMods.reduceRecoil);
            ImGui::Checkbox("Reduce Spread", &settings.weaponMods.reduceSpread);
            
            ImGui::Spacing();
            ImGui::SliderFloat("Custom FOV", &settings.weaponMods.customFov, 60.0f, 120.0f, "%.1f");
            
            ImGui::EndTabItem();
        }
        
        //========== SKIN CHANGER TAB ==========
        if (ImGui::BeginTabItem("Skin Changer")) {
            
            ImGui::Checkbox("Enable Skin Changer", &settings.skinChanger.enabled);
            
            if (settings.skinChanger.enabled) {
                ImGui::SameLine();
                ImGui::Checkbox("Apply to All Weapons", &settings.skinChanger.forceAllWeapons);
            }
            
            ImGui::Separator();
            
            if (settings.skinChanger.enabled) {
                // Weapon Selection
                ImGui::Text("Weapon Selection");
                ImGui::Combo("Weapon", &s_selectedWeapon, s_weaponNames, IM_ARRAYSIZE(s_weaponNames));
                
                ImGui::Spacing();
                
                // Skin Selection
                ImGui::Text("Skin Selection");
                ImGui::Combo("Skin", &s_selectedSkin, s_skinNames, IM_ARRAYSIZE(s_skinNames));
                
                ImGui::Spacing();
                
                // Wear Selection
                ImGui::Text("Wear Condition");
                int wearIndex = 0;
                for (int i = 0; i < 5; i++) {
                    if (s_wearValue >= s_wearValues[i] - 0.01f && s_wearValue <= s_wearValues[i] + 0.01f) {
                        wearIndex = i;
                        break;
                    }
                }
                if (ImGui::Combo("Wear", &wearIndex, s_wearNames, IM_ARRAYSIZE(s_wearNames))) {
                    s_wearValue = s_wearValues[wearIndex];
                }
                
                ImGui::Spacing();
                
                // Pattern Seed
                ImGui::Text("Pattern Seed (affects skin pattern)");
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("Seed", &s_seedValue, 1, 10);
                if (s_seedValue < 0) s_seedValue = 0;
                if (s_seedValue > 1000) s_seedValue = 1000;
                
                ImGui::Spacing();
                
                // StatTrak
                ImGui::Checkbox("Enable StatTrak", &s_statTrakEnabled);
                if (s_statTrakEnabled) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputInt("Kills", &s_statTrakCount, 1, 100);
                    if (s_statTrakCount < 0) s_statTrakCount = 0;
                    if (s_statTrakCount > 999999) s_statTrakCount = 999999;
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                
                // Apply Button
                if (ImGui::Button("Apply Skin", ImVec2(120, 30))) {
                    // Apply settings
                    settings.skinChanger.selectedWeapon = s_selectedWeapon;
                    settings.skinChanger.selectedSkin = s_selectedSkin;
                    settings.skinChanger.wear = s_wearValue;
                    settings.skinChanger.seed = s_seedValue;
                    settings.skinChanger.enableStatTrak = s_statTrakEnabled;
                    settings.skinChanger.statTrakCount = s_statTrakCount;
                    
                    // Get skin changer engine and apply
                    auto& skinEngine = SkinChanger::SkinChangerEngine::Instance();
                    // Use BloodStrike AK47 skin as base, offset by selected skin index
                    uint32_t skinId = SkinChanger::Skins::AK47::SKIN_101100029 + s_selectedSkin; // BloodStrike skin mapping
                    
                    if (settings.skinChanger.forceAllWeapons) {
                        skinEngine.ApplySkinToAllWeapons(skinId, s_wearValue, s_seedValue);
                    } else {
                        skinEngine.ApplySkinToCurrentWeapon(skinId, s_wearValue, s_seedValue);
                    }
                }
                
                ImGui::SameLine();
                
                if (ImGui::Button("Reset to Default", ImVec2(120, 30))) {
                    s_selectedWeapon = 0;
                    s_selectedSkin = 0;
                    s_wearValue = 0.0f;
                    s_seedValue = 0;
                    s_statTrakEnabled = false;
                    s_statTrakCount = 9999;
                }
                
                // Info text
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                    "Note: Pick up weapon or switch to apply skin changes.");
            }
            
            ImGui::EndTabItem();
        }
        
        //========== CONFIGURATION TAB ==========
        if (ImGui::BeginTabItem("Configuration")) {
            
            ImGui::Text("Settings");
            ImGui::Separator();
            
            ImGui::SliderFloat("Overlay Opacity", &settings.overlay.opacity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderInt("Font Size", &settings.overlay.fontSize, 10, 20);
            
            ImGui::Spacing();
            
            if (ImGui::Button("Save Config")) {
                // TODO: Implement config save
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Config")) {
                // TODO: Implement config load
            }
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void HandleInput() {
    // Toggle menu with Insert key
    if (Input::IsKeyPressed(Input::Key::Insert)) {
        Toggle();
    }
}

void Toggle() {
    s_showMenu = !s_showMenu;
}

bool IsOpen() {
    return s_showMenu;
}

} // namespace Menu