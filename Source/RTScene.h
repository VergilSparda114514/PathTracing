#pragma once

#include "framework/VulkanHelpers.h"

#include "RTAccelerationStructure.h"
#include "RTMesh.h"
#include "RTMaterial.h"

class RTScene
{
public:
    void Destroy(VkDevice device);

    void Load(const std::filesystem::path& scenePath);

    void BuildTLAS(VkDevice device, VkCommandPool cmdPool, VkQueue queue);
    void UpdateTLAS(VkCommandBuffer commandBuffer);

    const RTAccelerationStructure& GetTLAS() const { return m_TLAS; }
    std::vector<RTMesh>& GetMeshes() { return m_Meshes; }
    const std::vector<RTMesh>& GetMeshes() const { return m_Meshes; }
    const std::vector<RTMaterial>& GetMaterials() const { return m_Materials; }

    const VulkanHelpers::Buffer& GetInstancesBuffer() const { return m_InstancesBuffer; }
    const VulkanHelpers::Buffer& GetMaterialsBuffer() const { return m_MaterialsBuffer; }

    const std::vector<VkDescriptorBufferInfo>& GetAttribsBufferInfos() const { return m_AttribsBufferInfos; }
    const std::vector<VkDescriptorBufferInfo>& GetFacesBufferInfos() const { return m_FacesBufferInfos; }
    const std::vector<VkDescriptorImageInfo>& GetTexturesInfos() const { return m_TexturesInfos; }
    const std::vector<VkDescriptorImageInfo>& GetBumpMapsInfos() const { return m_BumpMapsInfos; }
private:
    std::vector<RTMesh>                   m_Meshes;
    std::vector<RTMaterial>               m_Materials;


    VkAccelerationStructureGeometryInstancesDataKHR m_TLASInstances{};
    VkAccelerationStructureGeometryKHR m_TLASGeometry{};
    VkAccelerationStructureBuildGeometryInfoKHR m_BuildInfo{};
    RTAccelerationStructure               m_TLAS;

    // Shader resources
    std::vector<VkDescriptorBufferInfo>   m_AttribsBufferInfos;
    std::vector<VkDescriptorBufferInfo>   m_FacesBufferInfos;
    VulkanHelpers::Buffer                 m_InstancesBuffer;
    VulkanHelpers::Buffer                 m_MaterialsBuffer;
    VulkanHelpers::Buffer                 m_ScratchBuffer;
    std::vector<VkDescriptorImageInfo>    m_TexturesInfos;
    std::vector<VkDescriptorImageInfo>    m_BumpMapsInfos;
};