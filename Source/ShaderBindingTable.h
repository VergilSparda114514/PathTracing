#pragma once

#include "framework/VulkanHelpers.h"

class ShaderBindingTable
{
public:
    void        Initialize(uint32_t numHitGroups, uint32_t numMissGroups, uint32_t shaderHandleSize, uint32_t shaderGroupAlignment);
    void        Destroy();
    void        SetRaygenStage(const VkPipelineShaderStageCreateInfo& stage);
    void        AddStageToHitGroup(const std::vector<VkPipelineShaderStageCreateInfo>& stages, uint32_t groupIndex);
    void        AddStageToMissGroup(const VkPipelineShaderStageCreateInfo& stage, uint32_t groupIndex);

    uint32_t    GetGroupsStride() const;
    uint32_t    GetNumGroups() const;
    uint32_t    GetRaygenOffset() const;
    uint32_t    GetRaygenSize() const;
    uint32_t    GetHitGroupsOffset() const;
    uint32_t    GetHitGroupsSize() const;
    uint32_t    GetMissGroupsOffset() const;
    uint32_t    GetMissGroupsSize() const;

    uint32_t                                    GetNumStages() const;
    const VkPipelineShaderStageCreateInfo* GetStages() const;
    const VkRayTracingShaderGroupCreateInfoKHR* GetGroups() const;

    uint32_t    GetSBTSize() const;
    bool        CreateSBT(VkDevice device, VkPipeline rtPipeline);
    VkDeviceAddress GetSBTAddress() const;
private:
    uint32_t                                    m_ShaderHandleSize = 0u;
    uint32_t                                    m_ShaderGroupAlignment = 0u;
    uint32_t                                    m_NumHitGroups = 0u;
    uint32_t                                    m_NumMissGroups = 0u;
    std::vector<uint32_t>                             m_NumHitShaders;
    std::vector<uint32_t>                             m_NumMissShaders;
    std::vector<VkPipelineShaderStageCreateInfo>      m_Stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_Groups;
    VulkanHelpers::Buffer                       m_SBTBuffer;
};