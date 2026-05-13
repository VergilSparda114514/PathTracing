#pragma once

#include "framework/VulkanHelpers.h"
#include "framework/Application.h"
#include "framework/Camera.h"

#define NUM_SETS 6

#include "RTScene.h"
#include "ShaderBindingTable.h"

class RTXApp : public Application
{
protected:
    virtual void InitSettings() override;
    virtual void InitApp() override;
    virtual void FreeResources() override;
    virtual void FillCommandBuffer(VkCommandBuffer commandBuffer, size_t imageIndex) override;

    virtual void OnUIRender(float deltaTime) override;
    virtual void OnUpdate(size_t imageIndex, float dt) override;
    virtual void OnResize() override;
private:
    enum class ToneMappingMode
    {
        None = 0,
        Neutral = 1,
        ACES = 2,
    };
private:
    void CreateScene();
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
    VulkanHelpers::Image            m_EnvTexture;
    VkDescriptorImageInfo           m_EnvTextureDescInfo;

    // Rendering
    VulkanHelpers::Image            m_ResultImage;
    VulkanHelpers::Image            m_PingImage;
    VulkanHelpers::Image            m_PongImage;
    VkDescriptorImageInfo           m_PingDescInfo;
    VkDescriptorImageInfo           m_PongDescInfo;

    // Camera & user input
    Camera                          m_Camera{ 60.0f, 0.1f, 100.0f };
    VulkanHelpers::Buffer           m_LightingBuffer;
    VulkanHelpers::Buffer           m_PostProcessBuffer;
    VulkanHelpers::Buffer           m_CurrResovoirBuffer;
    VulkanHelpers::Buffer           m_PrevResovoirBuffer;
    uint32_t                        m_Frame = 0;
    uint32_t                        m_AccumulatedFrame = 0;
};