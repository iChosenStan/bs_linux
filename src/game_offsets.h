#pragma once

#include <cstdint>
#include <cmath>

//=============================================================================
// BloodStrike Unreal Engine 4 Offsets
// Based on export_20260402_061600.json
//=============================================================================

namespace GameOffsets {

//=============================================================================
// Global Pointers (Need to be found via pattern scanning)
//=============================================================================
namespace Globals {
    // These are placeholder addresses - need pattern scanning
    // GWorld is typically found via pattern scan
    constexpr uintptr_t GWorld = 0x0;          // UWorld*
    constexpr uintptr_t GNames = 0x0;          // FNamePool*
    constexpr uintptr_t GUObjectArray = 0x0;   // FUObjectArray*
    
    // Game-specific base addresses (set at runtime)
    inline uintptr_t GameBase = 0;
    inline uintptr_t WorldPtr = 0;
}

//=============================================================================
// UObject Offsets (UE4 Standard from JSON)
//=============================================================================
namespace UObject {
    constexpr uintptr_t VTable = 0x0;
    constexpr uintptr_t ObjectFlags = 0x8;
    constexpr uintptr_t InternalIndex = 0xC;
    constexpr uintptr_t Class = 0x10;
    constexpr uintptr_t Name = 0x18;
    constexpr uintptr_t Outer = 0x20;
}

//=============================================================================
// UClass Offsets (from JSON)
//=============================================================================
namespace UClass {
    constexpr uintptr_t SuperClass = 0x28;
    constexpr uintptr_t PropertiesSize = 0x54;
    constexpr uintptr_t Children = 0x30;
}

//=============================================================================
// UProperty Offsets (from JSON)
//=============================================================================
namespace UProperty {
    constexpr uintptr_t ArrayDim = 0x38;
    constexpr uintptr_t ElementSize = 0x3C;
    constexpr uintptr_t PropertyFlags = 0x40;
    constexpr uintptr_t Offset_Internal = 0x4C;
}

//=============================================================================
// AActor Offsets (UE4 Standard)
//=============================================================================
namespace AActor {
    constexpr uintptr_t RootComponent = 0x30;
    constexpr uintptr_t Owner = 0x38;
    constexpr uintptr_t Instigator = 0x40;
    constexpr uintptr_t ActorId = 0x48;
}

//=============================================================================
// APawn Offsets
//=============================================================================
namespace APawn {
    constexpr uintptr_t PlayerState = 0x2A0;
    constexpr uintptr_t Controller = 0x2A8;
    constexpr uintptr_t MovementComponent = 0x2B0;
}

//=============================================================================
// APlayerController Offsets
//=============================================================================
namespace APlayerController {
    constexpr uintptr_t Pawn = 0x2A0;
    constexpr uintptr_t PlayerState = 0x2B0;
    constexpr uintptr_t PlayerCameraManager = 0x2C0;
    constexpr uintptr_t ControlRotation = 0x2D0;
}

//=============================================================================
// USceneComponent Offsets (for position)
//=============================================================================
namespace USceneComponent {
    constexpr uintptr_t RelativeLocation = 0x1A0;
    constexpr uintptr_t RelativeRotation = 0x1B0;
    constexpr uintptr_t ComponentToWorld = 0x1D0;
}

//=============================================================================
// USkinnedMeshComponent Offsets (for bones)
//=============================================================================
namespace USkinnedMeshComponent {
    constexpr uintptr_t SkeletalMesh = 0x100;
    constexpr uintptr_t ComponentSpaceTransforms = 0x200;
    constexpr uintptr_t BoneMatrixArray = 0x210;
}

//=============================================================================
// UWorld Offsets
//=============================================================================
namespace UWorld {
    constexpr uintptr_t PersistentLevel = 0x30;
    constexpr uintptr_t GameInstance = 0x98;
}

//=============================================================================
// ULevel Offsets
//=============================================================================
namespace ULevel {
    constexpr uintptr_t Actors = 0x98;
    constexpr uintptr_t ActorsCount = 0xA0;
}

//=============================================================================
// Bone IDs (BloodStrike - may need adjustment)
//=============================================================================
namespace Bones {
    // Bone IDs - using uppercase for compatibility with existing code
    enum BoneID {
        Root = 0,
        PELVIS = 1,
        SPINE = 2,
        SPINE1 = 3,
        SPINE2 = 4,
        SPINE3 = 5,
        NECK = 6,
        HEAD = 7,
        FACE = 8,
        
        // Left arm
        LEFT_CLAVICLE = 9,
        LEFT_UPPER_ARM = 10,
        LEFT_FOREARM = 11,
        LEFT_HAND = 12,
        
        // Right arm
        RIGHT_CLAVICLE = 13,
        RIGHT_UPPER_ARM = 14,
        RIGHT_FOREARM = 15,
        RIGHT_HAND = 16,
        
        // Left leg
        LEFT_THIGH = 17,
        LEFT_CALF = 18,
        LEFT_FOOT = 19,
        LEFT_TOE = 20,
        
        // Right leg
        RIGHT_THIGH = 21,
        RIGHT_CALF = 22,
        RIGHT_FOOT = 23,
        RIGHT_TOE = 24,
        
        // Aliases for common names (lowercase versions)
        Pelvis = PELVIS,
        Spine = SPINE,
        Spine1 = SPINE1,
        Spine2 = SPINE2,
        Neck = NECK,
        Head = HEAD,
        LeftUpperArm = LEFT_UPPER_ARM,
        LeftLowerArm = LEFT_FOREARM,
        LeftHand = LEFT_HAND,
        RightUpperArm = RIGHT_UPPER_ARM,
        RightLowerArm = RIGHT_FOREARM,
        RightHand = RIGHT_HAND,
        LeftUpperLeg = LEFT_THIGH,
        LeftLowerLeg = LEFT_CALF,
        LeftFoot = LEFT_FOOT,
        RightUpperLeg = RIGHT_THIGH,
        RightLowerLeg = RIGHT_CALF,
        RightFoot = RIGHT_FOOT,
        
        BONE_COUNT = 64  // Maximum bones
    };
}

//=============================================================================
// Pattern Signatures
//=============================================================================
namespace Patterns {
    constexpr const char* GWorldSig = "48 8B 05 ? ? ? ? 48 8B 88";
    constexpr const char* GNamesSig = "48 8B 05 ? ? ? ? 48 85 C0";
}

//=============================================================================
// Entity Structure Offsets (for entity_manager)
//=============================================================================
namespace Entity {
    constexpr int MAX_ENTITIES = 128;
    
    namespace Structure {
        constexpr uintptr_t ENTITY_ID = 0x0;
        constexpr uintptr_t ENTITY_TYPE = 0x4;
        constexpr uintptr_t TEAM_ID = 0x8;
        constexpr uintptr_t HEALTH = 0xC;
        constexpr uintptr_t MAX_HEALTH = 0x10;
        constexpr uintptr_t ARMOR = 0x14;
        constexpr uintptr_t IS_ALIVE = 0x18;
        constexpr uintptr_t IS_CROUCHING = 0x19;
        constexpr uintptr_t IS_PRONE = 0x1A;
        constexpr uintptr_t IS_RELOADING = 0x1B;
        constexpr uintptr_t IS_VISIBLE = 0x1C;
        constexpr uintptr_t POSITION = 0x20;
        constexpr uintptr_t ROTATION = 0x2C;
        constexpr uintptr_t VELOCITY = 0x38;
        constexpr uintptr_t HEAD_POSITION = 0x44;
        constexpr uintptr_t PLAYER_NAME = 0x50;
        constexpr uintptr_t PLAYER_LEVEL = 0x70;
        constexpr uintptr_t WEAPON_ID = 0x74;
        constexpr uintptr_t BONE_MATRIX = 0x78;
    }
}

//=============================================================================
// Game Manager Offsets
//=============================================================================
namespace GameManager {
    enum class GameState {
        MENU = 0,
        LOADING = 1,
        PLAYING = 2,
        PAUSED = 3,
        GAME_OVER = 4
    };
    
    namespace Structure {
        constexpr uintptr_t GAME_STATE = 0x0;
        constexpr uintptr_t ENTITY_COUNT = 0x4;
        constexpr uintptr_t ENTITY_LIST = 0x8;
        constexpr uintptr_t LOCAL_PLAYER = 0x10;
    }
}

} // namespace GameOffsets

//=============================================================================
// Vector Types (UE4 Compatible)
//=============================================================================

struct FVector {
    float X, Y, Z;
    
    FVector() : X(0), Y(0), Z(0) {}
    FVector(float x, float y, float z) : X(x), Y(y), Z(z) {}
    
    FVector operator+(const FVector& other) const {
        return FVector(X + other.X, Y + other.Y, Z + other.Z);
    }
    
    FVector operator-(const FVector& other) const {
        return FVector(X - other.X, Y - other.Y, Z - other.Z);
    }
    
    FVector operator*(float scalar) const {
        return FVector(X * scalar, Y * scalar, Z * scalar);
    }
    
    float Dot(const FVector& other) const {
        return X * other.X + Y * other.Y + Z * other.Z;
    }
    
    float Length() const {
        return sqrtf(X * X + Y * Y + Z * Z);
    }
    
    float DistanceTo(const FVector& other) const {
        return (*this - other).Length();
    }
};

struct FVector2D {
    float X, Y;
    
    FVector2D() : X(0), Y(0) {}
    FVector2D(float x, float y) : X(x), Y(y) {}
};

struct FRotator {
    float Pitch, Yaw, Roll;
    
    FRotator() : Pitch(0), Yaw(0), Roll(0) {}
    FRotator(float p, float y, float r) : Pitch(p), Yaw(y), Roll(r) {}
};