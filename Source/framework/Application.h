#pragma once

#include "VulkanHelpers.h"

#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "GLFW/glfw3.h"

#include "Common.h"

constexpr int gMaxFramesInFlight = 3;

struct AppSettings
{
    std::string name = "Application";
    uint32_t    resolutionX = 1280;
    uint32_t    resolutionY = 960;
    VkFormat    surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
	bool        fullscreen = false;
    bool        enableValidation = false;
    bool        enableVSync = false;
    bool        supportRaytracing = false;
    bool        supportDescriptorIndexing = false;
    bool		supportDocking = false;
};

class Application
{
public:
    virtual ~Application();

    void    Run();
protected:
    void    LoadDynamicRenderingExtensionFunctions();
    bool    Initialize();
    void    Loop();
    void    Shutdown() const;

    bool    InitializeVulkan();
    void    InitializeImgui();
    bool    InitializeDevicesAndQueues();
    bool    InitializeSurface();
    bool    InitializeSwapchain();
    bool    InitializeFencesAndCommandPool();
    bool    InitializeOffscreenImage();
    bool    InitializeCommandBuffers();
    bool    InitializeSynchronization();
    void    FillCommandBuffers();
    void    CleanupSwapchain();
    void    RecreateSwapchain();

    void    ProcessFrame(float dt, ImDrawData* drawData);
    VkCommandBuffer RecordCommandBuffer(size_t imageIndex, ImDrawData* drawData);
    void    FreeVulkan();

    virtual void InitSettings() {}
    virtual void InitApp() {}
    virtual void FreeResources() {}
    virtual void FillCommandBuffer(VkCommandBuffer commandBuffer, size_t imageIndex) {}

    virtual void OnUIRender(float dt) {}
    virtual void OnUpdate(size_t imageIndex, float dt) {}
    virtual void OnResize() {}
public:
    bool mFramebufferResized = false;
protected:
    AppSettings             m_Settings{};
    GLFWwindow*             m_Window = nullptr;

    VkInstance              m_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice        m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice                m_Device = VK_NULL_HANDLE;
    VkSurfaceFormatKHR      m_SurfaceFormat{};
    VkSurfaceKHR            m_Surface = VK_NULL_HANDLE;
    VkSwapchainKHR          m_Swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>          m_SwapchainImages;
    std::vector<VkImageView>      m_SwapchainImageViews;
    std::vector<VkFence>          m_WaitForFrameFences;
    VkCommandPool           m_CommandPool = VK_NULL_HANDLE;
    VulkanHelpers::Image    m_OffscreenImage;
    std::vector<VkCommandBuffer>  m_CommandBuffers;
    VkSemaphore             m_SemaphoreImageAcquired = VK_NULL_HANDLE;
    VkSemaphore             m_SemaphoreRenderFinished = VK_NULL_HANDLE;
    uint32_t                m_CurrentFrame = 0;
    uint32_t                m_ViewportWidth = 0;
    uint32_t                m_ViewportHeight = 0;

    uint32_t                m_GraphicsQueueFamilyIndex = 0u;
    uint32_t                m_ComputeQueueFamilyIndex = 0u;
    uint32_t                m_TransferQueueFamilyIndex = 0u;
    VkQueue                 m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue                 m_ComputeQueue = VK_NULL_HANDLE;
    VkQueue                 m_TransferQueue = VK_NULL_HANDLE;

    // RTX
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RTProperties;
};