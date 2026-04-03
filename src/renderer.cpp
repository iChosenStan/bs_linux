#include "renderer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <cstdio>
#include <vector>
#include <array>

// Handle older GLFW versions that don't have GLFW_MOUSE_PASSTHROUGH
#ifndef GLFW_MOUSE_PASSTHROUGH
    #define GLFW_MOUSE_PASSTHROUGH 0x0002000D
#endif

namespace Renderer {

static VulkanContext s_ctx;
static GLFWwindow* s_window = nullptr;
static float s_screenWidth = 1920.0f;
static float s_screenHeight = 1080.0f;

// Color conversion helper
inline ImVec4 ColorToImVec4(uint32_t color) {
    return ImVec4(
        ((color >> 0) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 24) & 0xFF) / 255.0f
    );
}

// Vulkan validation callback
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    fprintf(stderr, "Vulkan validation: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

bool Initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    
    // Tell GLFW not to create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // Always on top
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);
    
    // Get primary monitor resolution
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    s_screenWidth = static_cast<float>(mode->width);
    s_screenHeight = static_cast<float>(mode->height);
    
    // Create window (transparent overlay)
    s_window = glfwCreateWindow(
        static_cast<int>(s_screenWidth),
        static_cast<int>(s_screenHeight),
        "BloodStrike Overlay",
        nullptr,
        nullptr
    );
    
    if (!s_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }
    
    // Position at origin
    glfwSetWindowPos(s_window, 0, 0);
    
    // Create Vulkan instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "BloodStrike Overlay";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    
    // Get GLFW required extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    // Enable validation layers in debug
    const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
    #ifdef DEBUG
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;
    #endif
    
    if (vkCreateInstance(&createInfo, nullptr, &s_ctx.instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        return false;
    }
    
    // Create surface
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(s_ctx.instance, s_window, nullptr, &surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create window surface\n");
        return false;
    }
    
    // Pick physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(s_ctx.instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(s_ctx.instance, &deviceCount, devices.data());
    s_ctx.physicalDevice = devices[0]; // Just pick first GPU
    
    // Find queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(s_ctx.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(s_ctx.physicalDevice, &queueFamilyCount, queueFamilies.data());
    
    uint32_t graphicsFamily = 0;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
            break;
        }
    }
    
    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    
    if (vkCreateDevice(s_ctx.physicalDevice, &deviceCreateInfo, nullptr, &s_ctx.device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        return false;
    }
    
    vkGetDeviceQueue(s_ctx.device, graphicsFamily, 0, &s_ctx.queue);
    
    // Initialize ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = s_ctx.instance;
    initInfo.PhysicalDevice = s_ctx.physicalDevice;
    initInfo.Device = s_ctx.device;
    initInfo.QueueFamily = graphicsFamily;
    initInfo.Queue = s_ctx.queue;
    initInfo.DescriptorPool = s_ctx.descriptorPool; // Would need to create this
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    // MSAASamples removed in newer ImGui versions - uses VK_SAMPLE_COUNT_1_BIT by default
    
    // ImGui backend initialization would go here
    // ImGui_ImplGlfw_InitForVulkan(s_window, true);
    // ImGui_ImplVulkan_Init(&initInfo, nullptr);
    
    return true;
}

void Shutdown() {
    // Cleanup ImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Cleanup Vulkan
    if (s_ctx.device) {
        vkDestroyDevice(s_ctx.device, nullptr);
    }
    if (s_ctx.instance) {
        vkDestroyInstance(s_ctx.instance, nullptr);
    }
    
    // Cleanup GLFW
    if (s_window) {
        glfwDestroyWindow(s_window);
    }
    glfwTerminate();
}

void NewFrame() {
    glfwPollEvents();
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Present() {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), s_ctx.commandBuffer);
}

void DrawLine(float x1, float y1, float x2, float y2, uint32_t color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddLine(
        ImVec2(x1, y1),
        ImVec2(x2, y2),
        color,
        1.0f
    );
}

void DrawRect(float x, float y, float w, float h, uint32_t color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddRect(
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        color,
        0.0f,
        0,
        1.0f
    );
}

void DrawRectFilled(float x, float y, float w, float h, uint32_t color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddRectFilled(
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        color,
        0.0f,
        0
    );
}

void DrawCircle(float x, float y, float radius, uint32_t color, int segments) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddCircle(
        ImVec2(x, y),
        radius,
        color,
        segments,
        1.0f
    );
}

void DrawText(float x, float y, const char* text, uint32_t color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddText(
        ImVec2(x, y),
        color,
        text
    );
}

float GetScreenWidth() {
    return s_screenWidth;
}

float GetScreenHeight() {
    return s_screenHeight;
}

VulkanContext& GetContext() {
    return s_ctx;
}

} // namespace Renderer