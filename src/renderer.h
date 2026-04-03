#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace Renderer {

// Initialize Vulkan renderer
bool Initialize();

// Cleanup
void Shutdown();

// Begin new frame
void NewFrame();

// Present frame
void Present();

// Drawing functions
void DrawLine(float x1, float y1, float x2, float y2, uint32_t color);
void DrawRect(float x, float y, float w, float h, uint32_t color);
void DrawRectFilled(float x, float y, float w, float h, uint32_t color);
void DrawCircle(float x, float y, float radius, uint32_t color, int segments = 32);
void DrawText(float x, float y, const char* text, uint32_t color);

// Screen dimensions
float GetScreenWidth();
float GetScreenHeight();

// Vulkan objects (for ImGui backend)
struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue queue;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    VkImage colorImage;
    VkImageView colorImageView;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    uint32_t imageIndex;
};

VulkanContext& GetContext();

} // namespace Renderer