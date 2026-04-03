#pragma once

#include <cstdint>
#include <functional>

namespace Hooks {

// Hook function types
using PresentHook = std::function<void()>;
using ResizeHook = std::function<void(int, int)>;
using InputHook = std::function<void(int, int, int)>;

// Initialize hooking system
void Initialize();

// Cleanup hooks
void Shutdown();

// Vulkan hooks
namespace Vulkan {
    // Hook Vulkan present function
    bool HookPresent(void* device);
    
    // Unhook present
    void UnhookPresent();
    
    // Original present function pointer
    extern void* OriginalPresent;
}

// Game function hooks
namespace Game {
    // Hook game functions for aimbot/ESP
    bool HookUpdate();
    bool HookRender();
}

// Trampoline-based inline hook
class InlineHook {
public:
    InlineHook() = default;
    ~InlineHook();
    
    bool Create(void* target, void* detour);
    void Remove();
    void* GetOriginal() const { return m_original; }
    
private:
    void* m_target = nullptr;
    void* m_detour = nullptr;
    void* m_original = nullptr;
    void* m_trampoline = nullptr;
    size_t m_hookSize = 0;
    uint8_t m_originalBytes[32] = {0};
};

} // namespace Hooks