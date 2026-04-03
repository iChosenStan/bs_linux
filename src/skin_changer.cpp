#include "skin_changer.h"
#include "game_offsets.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>

namespace SkinChanger {

//=============================================================================
// Static Instance
//=============================================================================

SkinChangerEngine& SkinChangerEngine::Instance() {
    static SkinChangerEngine instance;
    return instance;
}

//=============================================================================
// Initialization
//=============================================================================

bool SkinChangerEngine::Initialize(uintptr_t moduleBase) {
    if (m_initialized) return true;
    
    m_moduleBase = moduleBase;
    
    // Initialize skin database
    InitializeSkinDatabase();
    
    // Set up offsets (from game_offsets.h or pattern scanned)
    // These would be the actual offsets in the game's memory
    m_offsets.LocalPlayer = 0x0;      // Would be set from game_offsets
    m_offsets.WeaponManager = 0x0;
    m_offsets.CurrentWeapon = 0x0;
    m_offsets.WeaponSkinId = 0x0;
    m_offsets.WeaponWear = 0x0;
    m_offsets.WeaponSeed = 0x0;
    m_offsets.ForceUpdate = 0x0;
    
    m_initialized = true;
    return true;
}

void SkinChangerEngine::Shutdown() {
    m_initialized = false;
    m_moduleBase = 0;
    m_skins.clear();
    m_weapons.clear();
}

//=============================================================================
// Skin Database
//=============================================================================

void SkinChangerEngine::InitializeSkinDatabase() {
    // BloodStrike skin database - REAL skin IDs from UnknownCheats community
    m_skins = {
        // ==================================================================================
        // ASSAULT RIFLES
        // ==================================================================================
        
        // M4A1 Skins (Weapon ID: 1)
        {Skins::M4A1::SKIN_11100018, "M4A1 Skin 11100018", "M4A1", 2},
        {Skins::M4A1::SKIN_11100020, "M4A1 Skin 11100020", "M4A1", 2},
        {Skins::M4A1::SKIN_11100007, "M4A1 Skin 11100007", "M4A1", 2},
        {Skins::M4A1::SKIN_11100001, "M4A1 Skin 11100001", "M4A1", 3},
        {Skins::M4A1::SKIN_11100022, "M4A1 Skin 11100022", "M4A1", 2},
        {Skins::M4A1::SKIN_11100017, "M4A1 Skin 11100017", "M4A1", 2},
        {Skins::M4A1::SKIN_11100004, "M4A1 Skin 11100004", "M4A1", 1},
        {Skins::M4A1::SKIN_11199003, "M4A1 Skin 11199003", "M4A1", 3},
        
        // AK47 Skins (Weapon ID: 40)
        {Skins::AK47::SKIN_101100029, "AK47 Skin 101100029", "AK47", 3},
        {Skins::AK47::SKIN_101100026, "AK47 Skin 101100026", "AK47", 2},
        {Skins::AK47::SKIN_101100011, "AK47 Skin 101100011", "AK47", 2},
        {Skins::AK47::SKIN_101100005, "AK47 Skin 101100005", "AK47", 2},
        {Skins::AK47::SKIN_101100004, "AK47 Skin 101100004", "AK47", 1},
        {Skins::AK47::SKIN_1011990155, "AK47 Skin 1011990155", "AK47", 3},
        {Skins::AK47::SKIN_1011990152, "AK47 Skin 1011990152", "AK47", 3},
        {Skins::AK47::SKIN_1011990153, "AK47 Skin 1011990153", "AK47", 3},
        {Skins::AK47::SKIN_1011990154, "AK47 Skin 1011990154", "AK47", 3},
        
        // SCAR Skins (Weapon ID: 88)
        {Skins::SCAR::SKIN_231100021, "SCAR Skin 231100021", "SCAR", 2},
        {Skins::SCAR::SKIN_231100022, "SCAR Skin 231100022", "SCAR", 2},
        {Skins::SCAR::SKIN_231100004, "SCAR Skin 231100004", "SCAR", 1},
        {Skins::SCAR::SKIN_231100013, "SCAR Skin 231100013", "SCAR", 2},
        {Skins::SCAR::SKIN_231199009, "SCAR Skin 231199009", "SCAR", 3},
        {Skins::SCAR::SKIN_231199013, "SCAR Skin 231199013", "SCAR", 3},
        {Skins::SCAR::SKIN_231100012, "SCAR Skin 231100012", "SCAR", 2},
        {Skins::SCAR::SKIN_231100010, "SCAR Skin 231100010", "SCAR", 2},
        
        // AUG Skins (Weapon ID: 98)
        {Skins::AUG::SKIN_291100010, "AUG Skin 291100010", "AUG", 2},
        {Skins::AUG::SKIN_291100011, "AUG Skin 291100011", "AUG", 2},
        {Skins::AUG::SKIN_291199018, "AUG Skin 291199018", "AUG", 3},
        
        // KAG6 Skins (Weapon ID: 72)
        {Skins::KAG6::SKIN_141100003, "KAG6 Skin 141100003", "KAG6", 2},
        {Skins::KAG6::SKIN_141100005, "KAG6 Skin 141100005", "KAG6", 2},
        {Skins::KAG6::SKIN_141100014, "KAG6 Skin 141100014", "KAG6", 2},
        {Skins::KAG6::SKIN_141100012, "KAG6 Skin 141100012", "KAG6", 1},
        
        // ==================================================================================
        // SMGs
        // ==================================================================================
        
        // MP5 Skins (Weapon ID: 2)
        {Skins::MP5::SKIN_21100009, "MP5 Skin 21100009", "MP5", 2},
        {Skins::MP5::SKIN_21100011, "MP5 Skin 21100011", "MP5", 2},
        {Skins::MP5::SKIN_21100005, "MP5 Skin 21100005", "MP5", 1},
        {Skins::MP5::SKIN_21100006, "MP5 Skin 21100006", "MP5", 1},
        {Skins::MP5::SKIN_21199011, "MP5 Skin 21199011", "MP5", 3},
        {Skins::MP5::SKIN_21100002, "MP5 Skin 21100002", "MP5", 1},
        
        // VECTOR Skins (Weapon ID: 38)
        {Skins::VECTOR::SKIN_81199004, "VECTOR Skin 81199004", "VECTOR", 3},
        {Skins::VECTOR::SKIN_81100003, "VECTOR Skin 81100003", "VECTOR", 2},
        {Skins::VECTOR::SKIN_81100004, "VECTOR Skin 81100004", "VECTOR", 2},
        {Skins::VECTOR::SKIN_81100005, "VECTOR Skin 81100005", "VECTOR", 2},
        {Skins::VECTOR::SKIN_81100015, "VECTOR Skin 81100015", "VECTOR", 2},
        {Skins::VECTOR::SKIN_81199009, "VECTOR Skin 81199009", "VECTOR", 3},
        {Skins::VECTOR::SKIN_81199018, "VECTOR Skin 81199018", "VECTOR", 3},
        {Skins::VECTOR::SKIN_81199007, "VECTOR Skin 81199007", "VECTOR", 3},
        {Skins::VECTOR::SKIN_81199010, "VECTOR Skin 81199010", "VECTOR", 3},
        
        // URB Skins (Weapon ID: 75)
        {Skins::URB::SKIN_161100006, "URB Skin 161100006", "URB", 2},
        {Skins::URB::SKIN_161199013, "URB Skin 161199013", "URB", 3},
        {Skins::URB::SKIN_161199006, "URB Skin 161199006", "URB", 3},
        
        // INP9 Skins (Weapon ID: 76)
        {Skins::INP9::SKIN_171100005, "INP9 Skin 171100005", "INP9", 2},
        {Skins::INP9::SKIN_171100012, "INP9 Skin 171100012", "INP9", 2},
        {Skins::INP9::SKIN_171100003, "INP9 Skin 171100003", "INP9", 1},
        {Skins::INP9::SKIN_171100014, "INP9 Skin 171100014", "INP9", 2},
        {Skins::INP9::SKIN_171100008, "INP9 Skin 171100008", "INP9", 1},
        {Skins::INP9::SKIN_171199012, "INP9 Skin 171199012", "INP9", 3},
        
        // P90 Skins (Weapon ID: 90)
        {Skins::P90::SKIN_241100012, "P90 Skin 241100012", "P90", 2},
        {Skins::P90::SKIN_241100002, "P90 Skin 241100002", "P90", 1},
        
        // UZI Skins (Weapon ID: 110)
        {Skins::UZI::SKIN_3511990050, "UZI Skin 3511990050", "UZI", 3},
        
        // ==================================================================================
        // SNIPERS
        // ==================================================================================
        
        // M700 Skins (Weapon ID: 77)
        {Skins::M700::SKIN_181100019, "M700 Skin 181100019", "M700", 2},
        {Skins::M700::SKIN_181100001, "M700 Skin 181100001", "M700", 1},
        {Skins::M700::SKIN_181100020, "M700 Skin 181100020", "M700", 2},
        {Skins::M700::SKIN_181199011, "M700 Skin 181199011", "M700", 3},
        {Skins::M700::SKIN_181100011, "M700 Skin 181100011", "M700", 2},
        {Skins::M700::SKIN_181100008, "M700 Skin 181100008", "M700", 1},
        
        // KAR98 Skins (Weapon ID: 103)
        {Skins::KAR98::SKIN_331100003, "KAR98 Skin 331100003", "KAR98", 2},
        {Skins::KAR98::SKIN_331199006, "KAR98 Skin 331199006", "KAR98", 3},
        {Skins::KAR98::SKIN_331199003, "KAR98 Skin 331199003", "KAR98", 3},
        
        // KALA Skins (Weapon ID: 71)
        {Skins::KALA::SKIN_131100008, "KALA Skin 131100008", "KALA", 2},
        {Skins::KALA::SKIN_131100001, "KALA Skin 131100001", "KALA", 1},
        
        // VSS Skins (Weapon ID: 91)
        {Skins::VSS::SKIN_251100013, "VSS Skin 251100013", "VSS", 2},
        
        // ==================================================================================
        // SHOTGUNS & OTHERS
        // ==================================================================================
        
        // ORIGIN Skins (Weapon ID: 34)
        {Skins::ORIGIN::SKIN_61100002, "ORIGIN Skin 61100002", "ORIGIN", 1},
        {Skins::ORIGIN::SKIN_61100005, "ORIGIN Skin 61100005", "ORIGIN", 2},
        
        // MP155 Skins (Weapon ID: 79)
        {Skins::MP155::SKIN_201100002, "MP155 Skin 201100002", "MP155", 1},
        {Skins::MP155::SKIN_201100001, "MP155 Skin 201100001", "MP155", 1},
    };
    
    // BloodStrike weapon database - REAL weapon IDs from UnknownCheats community
    m_weapons = {
        // Assault Rifles
        {Weapons::M4A1, "M4A1"},
        {Weapons::AK47, "AK47"},
        {Weapons::SCAR, "SCAR"},
        {Weapons::AUG, "AUG"},
        {Weapons::KAG6, "KAG6"},
        
        // SMGs
        {Weapons::MP5, "MP5"},
        {Weapons::VECTOR, "VECTOR"},
        {Weapons::URB, "URB"},
        {Weapons::INP9, "INP9"},
        {Weapons::P90, "P90"},
        {Weapons::UZI, "UZI"},
        
        // Snipers
        {Weapons::M700, "M700"},
        {Weapons::KAR98, "KAR98"},
        {Weapons::KALA, "KALA"},
        {Weapons::VSS, "VSS"},
        
        // Shotguns & Others
        {Weapons::ORIGIN, "ORIGIN"},
        {Weapons::MP155, "MP155"},
    };
}

//=============================================================================
// Memory Operations
//=============================================================================

bool SkinChangerEngine::ReadMemory(uintptr_t address, void* buffer, size_t size) {
    // For self-process reading
    struct iovec local = { buffer, size };
    struct iovec remote = { reinterpret_cast<void*>(address), size };
    
    ssize_t bytesRead = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
    return bytesRead == static_cast<ssize_t>(size);
}

bool SkinChangerEngine::WriteMemory(uintptr_t address, const void* buffer, size_t size) {
    // For self-process writing
    struct iovec local = { const_cast<void*>(buffer), size };
    struct iovec remote = { reinterpret_cast<void*>(address), size };
    
    ssize_t bytesWritten = process_vm_writev(getpid(), &local, 1, &remote, 1, 0);
    return bytesWritten == static_cast<ssize_t>(size);
}

//=============================================================================
// Player/Weapon Access
//=============================================================================

uintptr_t SkinChangerEngine::GetLocalPlayer() {
    if (!m_initialized) return 0;
    
    // Read local player pointer from game memory
    // This would use offsets from game_offsets.h
    uintptr_t localPlayer = 0;
    
    // Example: ReadMemory(m_moduleBase + m_offsets.LocalPlayer, &localPlayer, sizeof(localPlayer));
    
    return localPlayer;
}

uintptr_t SkinChangerEngine::GetCurrentWeapon() {
    uintptr_t player = GetLocalPlayer();
    if (!player) return 0;
    
    uintptr_t weapon = 0;
    // Read current weapon from player structure
    // ReadMemory(player + m_offsets.CurrentWeapon, &weapon, sizeof(weapon));
    
    return weapon;
}

uintptr_t SkinChangerEngine::GetWeaponByIndex(int index) {
    uintptr_t weaponManager = 0;
    // Read weapon manager
    // ReadMemory(m_moduleBase + m_offsets.WeaponManager, &weaponManager, sizeof(weaponManager));
    
    if (!weaponManager) return 0;
    
    // Calculate weapon address by index
    uintptr_t weapon = weaponManager + (index * 0x10);  // Assuming 0x10 stride
    return weapon;
}

bool SkinChangerEngine::IsInGame() {
    return GetLocalPlayer() != 0;
}

//=============================================================================
// Skin Application
//=============================================================================

void SkinChangerEngine::WriteSkinInfo(uintptr_t weaponPtr, const WeaponSkinInfo& info) {
    if (!weaponPtr) return;
    
    // Write skin ID
    WriteMemory(weaponPtr + m_offsets.WeaponSkinId, &info.skinId, sizeof(info.skinId));
    
    // Write wear
    WriteMemory(weaponPtr + m_offsets.WeaponWear, &info.wear, sizeof(info.wear));
    
    // Write seed
    WriteMemory(weaponPtr + m_offsets.WeaponSeed, &info.seed, sizeof(info.seed));
    
    // Write stattrak if enabled
    if (m_settings.enableStatTrak) {
        WriteMemory(weaponPtr + 0x0, &m_settings.statTrakCount, sizeof(m_settings.statTrakCount));
    }
}

bool SkinChangerEngine::ApplySkinToWeapon(uint32_t weaponId, uint32_t skinId, float wear, uint32_t seed) {
    if (!m_initialized) return false;
    
    // Store in settings for persistence
    ActiveSkinConfig config;
    config.weaponId = weaponId;
    config.skinId = skinId;
    config.wear = wear;
    config.seed = seed;
    config.enabled = true;
    m_settings.weaponSkins[weaponId] = config;
    
    return true;
}

bool SkinChangerEngine::ApplySkinToCurrentWeapon(uint32_t skinId, float wear, uint32_t seed) {
    if (!m_initialized || !IsInGame()) return false;
    
    uintptr_t weapon = GetCurrentWeapon();
    if (!weapon) return false;
    
    WeaponSkinInfo info = {};
    info.skinId = skinId;
    info.wear = wear;
    info.seed = seed;
    info.stattrak = m_settings.enableStatTrak ? m_settings.statTrakCount : 0;
    
    WriteSkinInfo(weapon, info);
    ForceUpdate();
    
    return true;
}

bool SkinChangerEngine::ApplySkinToAllWeapons(uint32_t skinId, float wear, uint32_t seed) {
    if (!m_initialized) return false;
    
    // Store in settings for all weapons
    m_settings.forceAllWeapons = true;
    m_settings.defaultWear = wear;
    m_settings.defaultSeed = seed;
    
    // Apply to all weapon slots
    for (int i = 0; i < 8; i++) {  // Typical 8 weapon slots
        uintptr_t weapon = GetWeaponByIndex(i);
        if (weapon) {
            WeaponSkinInfo info = {};
            info.skinId = skinId;
            info.wear = wear;
            info.seed = seed;
            WriteSkinInfo(weapon, info);
        }
    }
    
    ForceUpdate();
    return true;
}

void SkinChangerEngine::ForceUpdate() {
    // This would call a game function to force visual update
    // Often done by:
    // 1. Changing to a different weapon and back
    // 2. Calling a game update function
    // 3. Writing to a specific memory location that triggers update
    
    if (!m_initialized || !IsInGame()) return;
    
    // Example: Write to force update address
    // int value = 1;
    // WriteMemory(m_moduleBase + m_offsets.ForceUpdate, &value, sizeof(value));
}

//=============================================================================
// Getters
//=============================================================================

const std::vector<SkinDefinition>& SkinChangerEngine::GetAvailableSkins() const {
    return m_skins;
}

const std::vector<std::pair<uint32_t, std::string>>& SkinChangerEngine::GetWeapons() const {
    return m_weapons;
}

uint32_t SkinChangerEngine::GetWeaponIdByName(const std::string& name) {
    for (const auto& weapon : m_weapons) {
        if (weapon.second == name) {
            return weapon.first;
        }
    }
    return 0;
}

uint32_t SkinChangerEngine::GetSkinIdByName(const std::string& name) {
    for (const auto& skin : m_skins) {
        if (skin.name == name) {
            return skin.id;
        }
    }
    return 0;
}

bool SkinChangerEngine::GetCurrentWeaponSkinInfo(WeaponSkinInfo& info) {
    uintptr_t weapon = GetCurrentWeapon();
    if (!weapon) return false;
    
    // Read current skin info
    ReadMemory(weapon + m_offsets.WeaponSkinId, &info.skinId, sizeof(info.skinId));
    ReadMemory(weapon + m_offsets.WeaponWear, &info.wear, sizeof(info.wear));
    ReadMemory(weapon + m_offsets.WeaponSeed, &info.seed, sizeof(info.seed));
    
    return true;
}

//=============================================================================
// Frame-Based Update (Call this every frame)
//=============================================================================

// This would be called from the main cheat loop to apply skins dynamically
void UpdateSkinChanger() {
    auto& engine = SkinChangerEngine::Instance();
    auto& settings = engine.GetSettings();
    
    if (!settings.enabled) return;
    
    if (settings.forceAllWeapons) {
        // Apply default skin to current weapon
        engine.ApplySkinToCurrentWeapon(
            settings.weaponSkins.begin()->second.skinId,
            settings.defaultWear,
            settings.defaultSeed
        );
    } else {
        // Apply specific skin for current weapon type
        uintptr_t weapon = engine.GetCurrentWeapon();
        if (weapon) {
            // Get weapon ID and apply configured skin
            // This would need weapon ID detection
        }
    }
}

} // namespace SkinChanger