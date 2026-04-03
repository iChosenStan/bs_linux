/**
 * Entity Manager Implementation for BloodStrike
 */

#include "entity_manager.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <cfloat>
#include "offset_scanner.h"

// For Linux memory reading
#include <sys/uio.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <cctype>

namespace EntitySystem {

// ============================================================================
// MEMORY READING HELPERS
// ============================================================================
namespace {

// Process ID of target game
static pid_t targetPid = 0;

// Read memory from process
bool ReadProcessMemory(pid_t pid, uintptr_t address, void* buffer, size_t size) {
    if (pid == 0 || address == 0) return false;
    
    struct iovec local_iov;
    struct iovec remote_iov;
    
    local_iov.iov_base = buffer;
    local_iov.iov_len = size;
    remote_iov.iov_base = reinterpret_cast<void*>(address);
    remote_iov.iov_len = size;
    
    ssize_t bytesRead = process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);
    return bytesRead == static_cast<ssize_t>(size);
}

// Read specific types
template<typename T>
T ReadMemory(pid_t pid, uintptr_t address) {
    T value = T();
    ReadProcessMemory(pid, address, &value, sizeof(T));
    return value;
}

// Read vector types
void ReadVector3(pid_t pid, uintptr_t address, Vector3& vec) {
    ReadProcessMemory(pid, address, &vec, sizeof(Vector3));
}

void ReadString(pid_t pid, uintptr_t address, char* buffer, size_t maxSize) {
    if (address == 0) {
        buffer[0] = '\0';
        return;
    }
    
    // Read string pointer and then the string
    uintptr_t strPtr = ReadMemory<uintptr_t>(pid, address);
    if (strPtr != 0) {
        size_t len = ReadMemory<size_t>(pid, address + sizeof(uintptr_t));
        // Cap len to avoid OOB: need at least one byte for the NUL terminator
        if (len > 0 && len < maxSize - 1) {
            ReadProcessMemory(pid, strPtr, buffer, len);
            buffer[len] = '\0';
        } else {
            buffer[0] = '\0';
        }
    }
}

} // anonymous namespace

// ============================================================================
// ENTITY IMPLEMENTATION
// ============================================================================

bool Entity::UpdateFromMemory(uintptr_t entityAddr) {
    address = entityAddr;
    
    // Read entity ID and type
    entityId = ReadMemory<uint32_t>(targetPid, entityAddr + GameOffsets::Entity::Structure::ENTITY_ID);
    entityType = static_cast<EntityType>(ReadMemory<int>(targetPid, entityAddr + GameOffsets::Entity::Structure::ENTITY_TYPE));
    
    // Read team
    int teamId = ReadMemory<int>(targetPid, entityAddr + GameOffsets::Entity::Structure::TEAM_ID);
    team = static_cast<Team>(teamId);
    
    // Read status
    health = ReadMemory<float>(targetPid, entityAddr + GameOffsets::Entity::Structure::HEALTH);
    maxHealth = ReadMemory<float>(targetPid, entityAddr + GameOffsets::Entity::Structure::MAX_HEALTH);
    armor = ReadMemory<float>(targetPid, entityAddr + GameOffsets::Entity::Structure::ARMOR);
    isAlive = ReadMemory<bool>(targetPid, entityAddr + GameOffsets::Entity::Structure::IS_ALIVE);
    isCrouching = ReadMemory<bool>(targetPid, entityAddr + GameOffsets::Entity::Structure::IS_CROUCHING);
    isProne = ReadMemory<bool>(targetPid, entityAddr + GameOffsets::Entity::Structure::IS_PRONE);
    isReloading = ReadMemory<bool>(targetPid, entityAddr + GameOffsets::Entity::Structure::IS_RELOADING);
    isVisible = ReadMemory<bool>(targetPid, entityAddr + GameOffsets::Entity::Structure::IS_VISIBLE);
    
    // Read positions
    ReadVector3(targetPid, entityAddr + GameOffsets::Entity::Structure::POSITION, position);
    ReadVector3(targetPid, entityAddr + GameOffsets::Entity::Structure::ROTATION, rotation);
    ReadVector3(targetPid, entityAddr + GameOffsets::Entity::Structure::VELOCITY, velocity);
    ReadVector3(targetPid, entityAddr + GameOffsets::Entity::Structure::HEAD_POSITION, bonePositions[GameOffsets::Bones::Head]);
    
    // Read player info
    ReadString(targetPid, entityAddr + GameOffsets::Entity::Structure::PLAYER_NAME, playerName, sizeof(playerName));
    playerLevel = ReadMemory<int>(targetPid, entityAddr + GameOffsets::Entity::Structure::PLAYER_LEVEL);
    weaponId = ReadMemory<int>(targetPid, entityAddr + GameOffsets::Entity::Structure::WEAPON_ID);
    
    // Read bone matrix pointer and bone positions
    uintptr_t boneMatrixPtr = ReadMemory<uintptr_t>(targetPid, entityAddr + GameOffsets::Entity::Structure::BONE_MATRIX);
    if (boneMatrixPtr != 0) {
        for (int i = 0; i < Entity::MAX_BONES; i++) {
            uintptr_t boneAddr = boneMatrixPtr + (i * 0x30);  // Each bone is 0x30 bytes
            ReadVector3(targetPid, boneAddr, bonePositions[i]);
        }
    }
    
    return true;
}

// ============================================================================
// ENTITY MANAGER IMPLEMENTATION
// ============================================================================

bool EntityManager::Initialize() {
    // Find game process
    DIR* procDir = opendir("/proc");
    if (!procDir) return false;
    
    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        
        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        
        // Check process name
        char commPath[64];
        snprintf(commPath, sizeof(commPath), "/proc/%d/comm", pid);
        
        FILE* commFile = fopen(commPath, "r");
        if (!commFile) continue;
        
        char comm[256];
        if (fgets(comm, sizeof(comm), commFile)) {
            comm[strcspn(comm, "\n")] = 0;
            
            // Check for BloodStrike process names
            if (strstr(comm, "bloodstrike") || strstr(comm, "BloodStrike") ||
                strstr(comm, "BSGame") || strstr(comm, "bsgame")) {
                targetPid = pid;
                fclose(commFile);
                break;
            }
        }
        fclose(commFile);
    }
    closedir(procDir);
    
    if (targetPid == 0) {
        std::cerr << "BloodStrike process not found" << std::endl;
        return false;
    }
    
    std::cout << "Found BloodStrike process: PID " << targetPid << std::endl;
    
    // Find base address and UE4 globals using OffsetScanner
    OffsetScanner::OffsetScannerEngine scanner;
    if (scanner.Initialize(targetPid)) {
        std::cout << "[EntityManager] Starting dynamic offset scan..." << std::endl;
        
        // Add default UE4 patterns
        scanner.AddPatterns(OffsetScanner::UE4Patterns::GetAllKnownPatterns());
        
        if (scanner.RunFullScan()) {
            auto discovered = scanner.GetOffsets();
            
            if (discovered.count("GWorld")) {
                GameOffsets::Globals::WorldPtr = discovered["GWorld"];
                std::cout << "[EntityManager] Found GWorld: 0x" << std::hex << GameOffsets::Globals::WorldPtr << std::dec << std::endl;
            }
            
            if (discovered.count("GNames")) {
                // GNames often needs special handling depending on whether it's FNamePool or older structure
                std::cout << "[EntityManager] Found GNames: 0x" << std::hex << discovered["GNames"] << std::dec << std::endl;
            }
            
            if (discovered.count("GUObjectArray")) {
                 std::cout << "[EntityManager] Found GUObjectArray: 0x" << std::hex << discovered["GUObjectArray"] << std::dec << std::endl;
            }
            
            if (discovered.count("ViewMatrix")) {
                viewMatrixAddr = discovered["ViewMatrix"];
                std::cout << "[EntityManager] Found ViewMatrix: 0x" << std::hex << viewMatrixAddr << std::dec << std::endl;
            }
        } else {
            std::cerr << "[EntityManager] Offset scan failed or was incomplete" << std::endl;
        }
    } else {
        std::cerr << "[EntityManager] Failed to initialize offset scanner for PID " << targetPid << std::endl;
    }
    
    entities.reserve(256); // Sensible default
    initialized = true;
    return true;
}

void EntityManager::Update() {
    if (!initialized || targetPid == 0 || GameOffsets::Globals::WorldPtr == 0) return;
    
    std::lock_guard<std::mutex> lock(entityMutex);
    updating = true;
    
    // 1. Resolve World and Level
    uintptr_t world = ReadMemory<uintptr_t>(targetPid, GameOffsets::Globals::WorldPtr);
    if (!world) {
        updating = false;
        return;
    }
    
    uintptr_t level = ReadMemory<uintptr_t>(targetPid, world + GameOffsets::UWorld::PersistentLevel);
    if (!level) {
        updating = false;
        return;
    }
    
    // 2. Read Actor List
    uintptr_t actorList = ReadMemory<uintptr_t>(targetPid, level + GameOffsets::ULevel::Actors);
    int actorCount = ReadMemory<int>(targetPid, level + GameOffsets::ULevel::ActorsCount);
    
    if (actorList && actorCount > 0 && actorCount < 10000) {
        std::vector<Entity> newEntities;
        newEntities.reserve(actorCount);
        
        for (int i = 0; i < actorCount; i++) {
            uintptr_t actorAddr = ReadMemory<uintptr_t>(targetPid, actorList + (i * sizeof(uintptr_t)));
            if (!actorAddr) continue;
            
            Entity entity;
            if (ReadEntity(actorAddr, entity)) {
                // Don't add local player to entity list
                if (localPlayerValid && actorAddr == localPlayer.address) continue;
                newEntities.push_back(entity);
            }
        }
        entities = std::move(newEntities);
    }
    
    // 3. Update Local Player
    ReadLocalPlayer();
    
    // 4. Read View Matrix
    ReadViewMatrix();
    
    updating = false;
}

bool EntityManager::ReadEntity(uintptr_t address, Entity& entity) {
    if (address == 0) return false;
    return entity.UpdateFromMemory(address);
}

bool EntityManager::ReadLocalPlayer() {
    uintptr_t world = ReadMemory<uintptr_t>(targetPid, GameOffsets::Globals::WorldPtr);
    if (!world) return false;
    
    uintptr_t gameInstance = ReadMemory<uintptr_t>(targetPid, world + GameOffsets::UWorld::GameInstance);
    if (!gameInstance) return false;
    
    uintptr_t localPlayers = ReadMemory<uintptr_t>(targetPid, gameInstance + 0x38); // Standard UE4 LocalPlayers array
    if (!localPlayers) return false;
    
    uintptr_t localPlayerPtr = ReadMemory<uintptr_t>(targetPid, localPlayers);
    if (!localPlayerPtr) return false;
    
    uintptr_t playerController = ReadMemory<uintptr_t>(targetPid, localPlayerPtr + 0x30); // Standard UE4 PlayerController
    if (!playerController) return false;
    
    uintptr_t pawn = ReadMemory<uintptr_t>(targetPid, playerController + GameOffsets::APlayerController::Pawn);
    if (!pawn) return false;
    
    localPlayerValid = ReadEntity(pawn, localPlayer);
    
    // If we successfully read the pawn, also update the view angles
    if (localPlayerValid) {
        localPlayer.viewAngle = ReadMemory<Vector2>(targetPid, playerController + GameOffsets::APlayerController::ControlRotation);
    }
    
    return localPlayerValid;
}

bool EntityManager::ReadViewMatrix() {
    if (viewMatrixAddr == 0) return false;
    return ReadProcessMemory(targetPid, viewMatrixAddr, &viewMatrix, sizeof(Matrix4x4));
}

void EntityManager::UpdateScreenPositions(int screenW, int screenH) {
    for (auto& entity : entities) {
        // Calculate distance from local player
        if (localPlayerValid) {
            entity.distance = localPlayer.position.Distance(entity.position);
        }
        
        // World to screen projection
        entity.onScreen = viewMatrix.WorldToScreen(entity.position, entity.screenPos, screenW, screenH);
        
        // Head position
        Vector3 headPos = entity.GetBonePosition(GameOffsets::Bones::Head);
        entity.headOnScreen = viewMatrix.WorldToScreen(headPos, entity.headScreenPos, screenW, screenH);
    }
}

size_t EntityManager::GetActiveEntityCount() const {
    size_t count = 0;
    for (const auto& entity : entities) {
        if (entity.isAlive) count++;
    }
    return count;
}

std::vector<Entity*> EntityManager::GetFilteredEntities(const EntityFilter& filter) {
    std::vector<Entity*> result;
    
    for (auto& entity : entities) {
        if (filter.PassesFilter(entity)) {
            result.push_back(&entity);
        }
    }
    
    return result;
}

Entity* EntityManager::FindBestTarget(const Vector3& localPos, const Vector2& viewAngles, 
                                       float maxFOV, bool prioritizeVisible) {
    Entity* best = nullptr;
    float bestScore = FLT_MAX;
    
    Team localTeam = localPlayerValid ? localPlayer.team : Team::TEAM_1;
    
    for (auto& entity : entities) {
        if (!entity.IsValidTarget()) continue;
        if (!entity.IsEnemy(localTeam)) continue;
        
        // Calculate angle to target
        Vector2 targetAngle = localPos.CalcAngle(entity.position);
        float yawDiff = std::abs(targetAngle.x - viewAngles.x);
        float pitchDiff = std::abs(targetAngle.y - viewAngles.y);
        float fovDist = std::max(yawDiff, pitchDiff);
        
        if (fovDist > maxFOV) continue;
        
        // Score based on FOV and visibility
        float score = fovDist;
        if (prioritizeVisible && entity.isVisible) {
            score *= 0.5f;  // Visible targets get priority
        }
        
        if (score < bestScore) {
            bestScore = score;
            best = &entity;
        }
    }
    
    return best;
}

} // namespace EntitySystem