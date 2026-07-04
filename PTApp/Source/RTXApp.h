#pragma once

#include "VulkanHelpers.h"
#include "Application.h"

#define NUM_SETS 6

#include "RTScene.h"
#include "ShaderBindingTable.h"

class RTXApplication final : public Application
{
protected:
    void InitSettings() override;
    void InitApp() override;
    void FreeResources() override;
    void FillCommandBuffer(VkCommandBuffer commandBuffer, size_t imageIndex) override;

    void OnUIRender(float deltaTime) override;
    void OnUpdate(size_t imageIndex, float dt) override;
    void OnResize() override;
private:
    void CreateBuffers();
    void CreateResultImage();
    void CreateRTDescriptorSetsLayouts();
    void UpdateRTDescriptorSets();
    void CreateRTPipelineAndSBT();
    void CreateComputePipeline();
private:
	// RT Pipeline
    std::array<VkDescriptorSetLayout, NUM_SETS>    m_RTDescriptorSetsLayouts{};
    VkPipelineLayout                m_RTPipelineLayout = VK_NULL_HANDLE;
    VkPipeline                      m_RTPipeline = VK_NULL_HANDLE;
    VkDescriptorPool                m_RTDescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, NUM_SETS>          m_RTDescriptorSets{};

    // Compute pipeline
    VulkanHelpers::ComputePass      m_FFTPaddingPass{};
    VulkanHelpers::ComputePass      m_FFTKernelPass{};
    VulkanHelpers::ComputePass      m_FFTPass{};
    std::vector<VulkanHelpers::ComputePass> m_ComputePasses{};

    ShaderBindingTable              m_SBT;

    RTScene                         m_Scene;

    // Rendering
    VulkanHelpers::Image            m_ResultImage{};
    VulkanHelpers::Image            m_KernelImage{};

    VulkanHelpers::Image            m_KernelPingImage{};
    VulkanHelpers::Image            m_KernelPongImage{};

    VulkanHelpers::Image            m_PingImage{};
    VulkanHelpers::Image            m_PongImage{};

    // Camera & user input
    VulkanHelpers::Buffer           m_LightingBuffer{};
    VulkanHelpers::Buffer           m_PostProcessBuffer{};
    VulkanHelpers::Buffer           m_CurrReservoirBuffer{};
    VulkanHelpers::Buffer           m_PrevReservoirBuffer{};
    uint32_t                        m_Frame = 0;
    uint32_t                        m_AccumulatedFrame = 0;
};