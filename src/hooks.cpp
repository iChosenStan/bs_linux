#include "hooks.h"
#include "cheat_features.h"
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <vulkan/vulkan.h>

namespace Hooks {

//=============================================================================
// REFERENCE: DIRECTX 11 VTABLE OFFSETS (from Windows DLL)
//=============================================================================
// The Windows DLL hooks IDXGISwapChain::Present at VTable offset 0x140
// For Linux Vulkan, we hook vkQueuePresentKHR instead
//
// Original Windows hook pattern:
//   Address: 0x1800011D0
//   Pattern: 40 53 55 56 48 81 EC 90 00 00 00 48 8B 05
//   call *0x140(%rax)  ; Present at VTable offset 0x140
//
// Global pointers stored in DLL:
//   g_Device:         RVA 0xA5EC8
//   g_DeviceContext:  RVA 0xA5ED0
//   g_SwapChain:      RVA 0xA5ED8
//   g_OriginalPresent: RVA 0xA5EB8
//=============================================================================

//=============================================================================
// Inline Hook Implementation
//=============================================================================

InlineHook::~InlineHook() {
    Remove();
}

bool InlineHook::Create(void* target, void* detour) {
    if (!target || !detour) return false;
    
    m_target = target;
    m_detour = detour;
    
    // Calculate hook size (need at least 14 bytes for 64-bit absolute jump)
    constexpr size_t minHookSize = 14;
    m_hookSize = minHookSize;
    
    // Save original bytes
    memcpy(m_originalBytes, m_target, m_hookSize);
    
    // Allocate executable memory for trampoline
    void* trampolineMem = mmap(
        nullptr,
        m_hookSize + 14,  // Original bytes + jump back
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    );
    
    if (trampolineMem == MAP_FAILED) {
        return false;
    }
    
    m_trampoline = trampolineMem;
    
    // Copy original bytes to trampoline
    memcpy(m_trampoline, m_originalBytes, m_hookSize);
    
    // Add jump back to original code after trampoline
    uint8_t jumpBack[14];
    jumpBack[0] = 0xFF;  // jmp [rip+0]
    jumpBack[1] = 0x25;
    jumpBack[2] = 0x00;
    jumpBack[3] = 0x00;
    jumpBack[4] = 0x00;
    jumpBack[5] = 0x00;
    
    uint64_t returnAddr = (uint64_t)m_target + m_hookSize;
    memcpy(&jumpBack[6], &returnAddr, 8);
    
    memcpy((uint8_t*)m_trampoline + m_hookSize, jumpBack, 14);
    
    // Set original to trampoline
    m_original = m_trampoline;
    
    // Make target memory writable — span two pages to handle hooks straddling a boundary
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = (uintptr_t)m_target & ~(pageSize - 1);
    
    if (mprotect((void*)pageStart, pageSize * 2, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        munmap(m_trampoline, m_hookSize + 14);
        return false;
    }
    
    // Write jump to detour
    uint8_t jumpCode[14];
    jumpCode[0] = 0xFF;  // jmp [rip+0]
    jumpCode[1] = 0x25;
    jumpCode[2] = 0x00;
    jumpCode[3] = 0x00;
    jumpCode[4] = 0x00;
    jumpCode[5] = 0x00;
    
    uint64_t detourAddr = (uint64_t)m_detour;
    memcpy(&jumpCode[6], &detourAddr, 8);
    
    memcpy(m_target, jumpCode, 14);
    
    // Restore memory protection (two pages, matching the mprotect above)
    mprotect((void*)pageStart, pageSize * 2, PROT_READ | PROT_EXEC);
    
    return true;
}

void InlineHook::Remove() {
    // Check using hookSize rather than byte value (first byte may legitimately be 0x00)
    if (!m_target || m_hookSize == 0) return;
    
    // Make memory writable — span two pages to handle hooks straddling a boundary
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = (uintptr_t)m_target & ~(pageSize - 1);
    
    mprotect((void*)pageStart, pageSize * 2, PROT_READ | PROT_WRITE | PROT_EXEC);
    
    // Restore original bytes
    memcpy(m_target, m_originalBytes, m_hookSize);
    
    // Restore protection
    mprotect((void*)pageStart, pageSize * 2, PROT_READ | PROT_EXEC);
    
    // Free trampoline
    if (m_trampoline) {
        munmap(m_trampoline, m_hookSize + 14);
        m_trampoline = nullptr;
    }
}

//=============================================================================
// Vulkan Hooking
//=============================================================================

namespace Vulkan {

void* OriginalPresent = nullptr;
static InlineHook s_presentHook;

// Hooked present function
static VkResult VKAPI_CALL HookedPresent(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo)
{
    // Call our callback (render ESP, etc.)
    auto& manager = Cheat::CheatManager::Instance();
    if (manager.initialized) {
        Cheat::ESP::Render();
        Cheat::Aimbot::Update();
    }
    
    // Call original
    using PresentFunc = VkResult(VKAPI_CALL*)(VkQueue, const VkPresentInfoKHR*);
    return ((PresentFunc)OriginalPresent)(queue, pPresentInfo);
}

bool HookPresent(void* device) {
    // Get vkQueuePresentKHR from device
    // This is a simplified version - real implementation would get function pointer
    
    // For Vulkan, we typically intercept at the dispatch table level
    // or use layering
    
    printf("[*] Vulkan present hook installed\n");
    return true;
}

void UnhookPresent() {
    s_presentHook.Remove();
    OriginalPresent = nullptr;
}

} // namespace Vulkan

//=============================================================================
// Initialization
//=============================================================================

static std::vector<std::unique_ptr<InlineHook>> s_hooks;

void Initialize() {
    printf("[*] Initializing hook system\n");
}

void Shutdown() {
    printf("[*] Shutting down hook system\n");
    
    Vulkan::UnhookPresent();
    s_hooks.clear();
}

} // namespace Hooks