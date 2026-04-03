#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace SkinChanger {

//=============================================================================
// Skin Definitions
//=============================================================================

// Popular BloodStrike skin IDs (these would need to be found via reverse engineering)
// Format: {skin_id, "Display Name"}
struct SkinDefinition {
    uint32_t id;
    std::string name;
    std::string weapon;
    int rarity;  // 0=Common, 1=Rare, 2=Epic, 3=Legendary
};

// Weapon info structure (matches game's internal structure)
#pragma pack(push, 1)
struct WeaponSkinInfo {
    uint32_t skinId;        // Skin ID (defines which skin)
    float wear;             // 0.0 = Factory New, 1.0 = Battle-Scarred
    uint32_t seed;          // Pattern seed (affects pattern orientation)
    uint32_t stattrak;      // StatTrak kill count (0 = no StatTrak)
    uint8_t  rank;          // Item wear rank (affects icon)
    uint8_t  pad[3];        // Padding
};
#pragma pack(pop)

// Active weapon skin config
struct ActiveSkinConfig {
    uint32_t weaponId;
    uint32_t skinId;
    float wear;
    uint32_t seed;
    bool stattrak;
    bool enabled;
};

//=============================================================================
// Settings Structure (for menu integration)
//=============================================================================

struct SkinChangerSettings {
    bool enabled = false;
    bool forceAllWeapons = false;  // Apply to all weapons
    
    // Per-weapon skin configs
    std::map<uint32_t, ActiveSkinConfig> weaponSkins;
    
    // Global settings
    float defaultWear = 0.0f;       // Factory New by default
    uint32_t defaultSeed = 0;       // Random seed
    bool enableStatTrak = false;
    uint32_t statTrakCount = 9999;
};

//=============================================================================
// Skin Changer Engine
//=============================================================================

class SkinChangerEngine {
public:
    static SkinChangerEngine& Instance();
    
    // Initialize with base address
    bool Initialize(uintptr_t moduleBase);
    void Shutdown();
    
    // Skin application
    bool ApplySkinToWeapon(uint32_t weaponId, uint32_t skinId, float wear = 0.0f, uint32_t seed = 0);
    bool ApplySkinToCurrentWeapon(uint32_t skinId, float wear = 0.0f, uint32_t seed = 0);
    bool ApplySkinToAllWeapons(uint32_t skinId, float wear = 0.0f, uint32_t seed = 0);
    
    // Force update - call this to make game reload weapon visuals
    void ForceUpdate();
    
    // Getters
    const std::vector<SkinDefinition>& GetAvailableSkins() const;
    const std::vector<std::pair<uint32_t, std::string>>& GetWeapons() const;
    
    // Settings
    SkinChangerSettings& GetSettings() { return m_settings; }
    void SetSettings(const SkinChangerSettings& settings) { m_settings = settings; }
    
    // Find weapon by name
    uint32_t GetWeaponIdByName(const std::string& name);
    uint32_t GetSkinIdByName(const std::string& name);
    
    // Get current weapon's skin info
    bool GetCurrentWeaponSkinInfo(WeaponSkinInfo& info);
    
    // Check if player is in-game
    bool IsInGame();
    
    // Get current weapon (public for frame update)
    uintptr_t GetCurrentWeapon();
    
private:
    SkinChangerEngine() = default;
    ~SkinChangerEngine() = default;
    
    // Internal helpers
    void InitializeSkinDatabase();
    uintptr_t GetLocalPlayer();
    uintptr_t GetWeaponByIndex(int index);
    void WriteSkinInfo(uintptr_t weaponPtr, const WeaponSkinInfo& info);
    
    // Memory operations
    bool ReadMemory(uintptr_t address, void* buffer, size_t size);
    bool WriteMemory(uintptr_t address, const void* buffer, size_t size);
    
private:
    bool m_initialized = false;
    uintptr_t m_moduleBase = 0;
    SkinChangerSettings m_settings;
    
    // Skin/Weapon databases
    std::vector<SkinDefinition> m_skins;
    std::vector<std::pair<uint32_t, std::string>> m_weapons;
    
    // Offsets (would be found via pattern scanning or hardcoded from game_offsets.h)
    struct Offsets {
        uintptr_t LocalPlayer;
        uintptr_t WeaponManager;
        uintptr_t CurrentWeapon;
        uintptr_t WeaponSkinId;
        uintptr_t WeaponWear;
        uintptr_t WeaponSeed;
        uintptr_t ForceUpdate;
    } m_offsets;
};

//=============================================================================
// Skin Database - REAL BloodStrike Skin IDs (from UnknownCheats community)
//=============================================================================

namespace Skins {
    // These are REAL BloodStrike skin IDs discovered by the UnknownCheats community
    // Source: https://www.unknowncheats.me/forum/other-fps-games/730162-bloodstrike-python-sdk-pc.html
    
    // ======================================================================================
    // SECTION 1 : ASSAULT RIFLES (AR)
    // ======================================================================================
    
    // M4A1 (Weapon ID: 1)
    namespace M4A1 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_11100018 = 11100018;
        constexpr uint32_t SKIN_11100020 = 11100020;
        constexpr uint32_t SKIN_11100007 = 11100007;
        constexpr uint32_t SKIN_11100001 = 11100001;
        constexpr uint32_t SKIN_11100022 = 11100022;
        constexpr uint32_t SKIN_11100017 = 11100017;
        constexpr uint32_t SKIN_11100004 = 11100004;
        constexpr uint32_t SKIN_11199003 = 11199003;
    }
    
    // AK47 (Weapon ID: 40)
    namespace AK47 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_101100029 = 101100029;
        constexpr uint32_t SKIN_101100026 = 101100026;
        constexpr uint32_t SKIN_101100011 = 101100011;
        constexpr uint32_t SKIN_101100005 = 101100005;
        constexpr uint32_t SKIN_101100004 = 101100004;
        constexpr uint32_t SKIN_1011990155 = 1011990155;
        constexpr uint32_t SKIN_1011990152 = 1011990152;
        constexpr uint32_t SKIN_1011990153 = 1011990153;
        constexpr uint32_t SKIN_1011990154 = 1011990154;
    }
    
    // SCAR (Weapon ID: 88)
    namespace SCAR {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_231100021 = 231100021;
        constexpr uint32_t SKIN_231100022 = 231100022;
        constexpr uint32_t SKIN_231100004 = 231100004;
        constexpr uint32_t SKIN_231100013 = 231100013;
        constexpr uint32_t SKIN_231199009 = 231199009;
        constexpr uint32_t SKIN_231199013 = 231199013;
        constexpr uint32_t SKIN_231100012 = 231100012;
        constexpr uint32_t SKIN_231100010 = 231100010;
    }
    
    // AUG (Weapon ID: 98)
    namespace AUG {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_291100010 = 291100010;
        constexpr uint32_t SKIN_291100011 = 291100011;
        constexpr uint32_t SKIN_291199018 = 291199018;
    }
    
    // KAG6 (Weapon ID: 72)
    namespace KAG6 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_141100003 = 141100003;
        constexpr uint32_t SKIN_141100005 = 141100005;
        constexpr uint32_t SKIN_141100014 = 141100014;
        constexpr uint32_t SKIN_141100012 = 141100012;
    }
    
    // ======================================================================================
    // SECTION 2 : SMG
    // ======================================================================================
    
    // MP5 (Weapon ID: 2)
    namespace MP5 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_21100009 = 21100009;
        constexpr uint32_t SKIN_21100011 = 21100011;
        constexpr uint32_t SKIN_21100005 = 21100005;
        constexpr uint32_t SKIN_21100006 = 21100006;
        constexpr uint32_t SKIN_21199011 = 21199011;
        constexpr uint32_t SKIN_21100002 = 21100002;
    }
    
    // VECTOR (Weapon ID: 38)
    namespace VECTOR {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_81199004 = 81199004;
        constexpr uint32_t SKIN_81100003 = 81100003;
        constexpr uint32_t SKIN_81100004 = 81100004;
        constexpr uint32_t SKIN_81100005 = 81100005;
        constexpr uint32_t SKIN_81100015 = 81100015;
        constexpr uint32_t SKIN_81199009 = 81199009;
        constexpr uint32_t SKIN_81199018 = 81199018;
        constexpr uint32_t SKIN_81199007 = 81199007;
        constexpr uint32_t SKIN_81199010 = 81199010;
    }
    
    // URB (Weapon ID: 75)
    namespace URB {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_161100006 = 161100006;
        constexpr uint32_t SKIN_161199013 = 161199013;
        constexpr uint32_t SKIN_161199006 = 161199006;
    }
    
    // INP9 (Weapon ID: 76)
    namespace INP9 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_171100005 = 171100005;
        constexpr uint32_t SKIN_171100012 = 171100012;
        constexpr uint32_t SKIN_171100003 = 171100003;
        constexpr uint32_t SKIN_171100014 = 171100014;
        constexpr uint32_t SKIN_171100008 = 171100008;
        constexpr uint32_t SKIN_171199012 = 171199012;
    }
    
    // P90 (Weapon ID: 90)
    namespace P90 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_241100012 = 241100012;
        constexpr uint32_t SKIN_241100002 = 241100002;
    }
    
    // UZI (Weapon ID: 110)
    namespace UZI {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_3511990050 = 3511990050;
    }
    
    // ======================================================================================
    // SECTION 3 : SNIPER
    // ======================================================================================
    
    // M700 (Weapon ID: 77)
    namespace M700 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_181100019 = 181100019;
        constexpr uint32_t SKIN_181100001 = 181100001;
        constexpr uint32_t SKIN_181100020 = 181100020;
        constexpr uint32_t SKIN_181199011 = 181199011;
        constexpr uint32_t SKIN_181100011 = 181100011;
        constexpr uint32_t SKIN_181100008 = 181100008;
    }
    
    // KAR98 (Weapon ID: 103)
    namespace KAR98 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_331100003 = 331100003;
        constexpr uint32_t SKIN_331199006 = 331199006;
        constexpr uint32_t SKIN_331199003 = 331199003;
    }
    
    // KALA (Weapon ID: 71)
    namespace KALA {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_131100008 = 131100008;
        constexpr uint32_t SKIN_131100001 = 131100001;
    }
    
    // VSS (Weapon ID: 91)
    namespace VSS {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_251100013 = 251100013;
    }
    
    // ======================================================================================
    // SECTION 4 : SHOTGUN & OTHERS
    // ======================================================================================
    
    // ORIGIN (Weapon ID: 34)
    namespace ORIGIN {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_61100002 = 61100002;
        constexpr uint32_t SKIN_61100005 = 61100005;
    }
    
    // MP155 (Weapon ID: 79)
    namespace MP155 {
        constexpr uint32_t DEFAULT = 0;
        constexpr uint32_t SKIN_201100002 = 201100002;
        constexpr uint32_t SKIN_201100001 = 201100001;
    }
}

//=============================================================================
// Weapon IDs - REAL BloodStrike Weapon IDs (from UnknownCheats community)
//=============================================================================

namespace Weapons {
    // ======================================================================================
    // ASSAULT RIFLES
    // ======================================================================================
    constexpr uint32_t M4A1 = 1;
    constexpr uint32_t MP5 = 2;
    constexpr uint32_t ORIGIN = 34;
    constexpr uint32_t VECTOR = 38;
    constexpr uint32_t AK47 = 40;
    constexpr uint32_t KALA = 71;
    constexpr uint32_t KAG6 = 72;
    constexpr uint32_t URB = 75;
    constexpr uint32_t INP9 = 76;
    constexpr uint32_t M700 = 77;
    constexpr uint32_t MP155 = 79;
    constexpr uint32_t P90 = 90;
    constexpr uint32_t VSS = 91;
    constexpr uint32_t SCAR = 88;
    constexpr uint32_t AUG = 98;
    constexpr uint32_t KAR98 = 103;
    constexpr uint32_t UZI = 110;
}

} // namespace SkinChanger