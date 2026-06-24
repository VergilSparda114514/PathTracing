#pragma once

#include "VulkanHelpers.h"
#include "Application.h"
#include "Camera.h"

#define NUM_SETS 6

#include "RTScene.h"
#include "ShaderBindingTable.h"

class RTXApp final : public Application
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
    void CreateComputeDescriptorSetsLayouts();
    void UpdateComputeDescriptorSets();
    void UpdatePingPongDescriptorSets(VkDescriptorImageInfo* ping, VkDescriptorImageInfo* pong);
    void CreateComputePipeline();
private:
	// RT Pipeline
    std::array<VkDescriptorSetLayout, NUM_SETS>    m_RTDescriptorSetsLayouts;
    VkPipelineLayout                m_RTPipelineLayout = VK_NULL_HANDLE;
    VkPipeline                      m_RTPipeline = VK_NULL_HANDLE;
    VkDescriptorPool                m_RTDescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, NUM_SETS>          m_RTDescriptorSets;

    // Compute pipeline
    // std::vector<std::unique_ptr<vulkanhelpers::ComputePassBase>>        mComputePasses;
    VkPipelineLayout                m_ComputePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool                m_ComputeDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout>    m_ComputeDescriptorSetsLayouts;
    std::vector<VkDescriptorSet>          m_ComputeDescriptorSets;
    VulkanHelpers::ComputePass      m_ThresholdPass;
    VulkanHelpers::ComputePass      m_DownsamplePass;
    VulkanHelpers::ComputePass      m_FFTPass;
    VulkanHelpers::ComputePass      m_UpsamplePass;
    VulkanHelpers::ComputePass      m_CompositePass;

    ShaderBindingTable              m_SBT;

    RTScene                         m_Scene;

    // Rendering
    VulkanHelpers::Image            m_ResultImage;
    VulkanHelpers::Image            m_KernelImage;
    VulkanHelpers::Image            m_PingImage;
    VulkanHelpers::Image            m_PongImage;
    VkDescriptorImageInfo           m_PingDescInfo;
    VkDescriptorImageInfo           m_PongDescInfo;

    // Camera & user input
    Camera                          m_Camera{ 60.0f, 0.1f, 100.0f };
    VulkanHelpers::Buffer           m_LightingBuffer;
    VulkanHelpers::Buffer           m_PostProcessBuffer;
    VulkanHelpers::Buffer           m_CurrReservoirBuffer;
    VulkanHelpers::Buffer           m_PrevReservoirBuffer;
    uint32_t                        m_Frame = 0;
    uint32_t                        m_AccumulatedFrame = 0;
};