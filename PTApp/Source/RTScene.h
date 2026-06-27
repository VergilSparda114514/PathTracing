#pragma once

#include "VulkanHelpers.h"

#include "RTAccelerationStructure.h"
#include "RTMesh.h"
#include "RTMaterial.h"

#include "Camera.h"

class RTScene
{
public:
    void Destroy(VkDevice device);

    void Init(VkDevice device, VkCommandPool cmdPool, VkQueue queue, const std::filesystem::path& scenePath, const std::filesystem::path& envPath);
    void Init(VkDevice device, VkCommandPool cmdPool, VkQueue queue, const std::filesystem::path& sceneFilePath);
    void Save();

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
	const VkDescriptorImageInfo& GetEnvTextureDescInfo() const { return m_EnvTextureDescInfo; }
public:
    Camera camera{ 60.0f, 0.1f, 100.0f };
private:
    void Load(VkDevice device, VkCommandPool cmdPool, VkQueue queue, const std::filesystem::path& scenePath, const std::filesystem::path& envPath);
    void LoadScene(const std::filesystem::path& scenePath);
private:
    std::filesystem::path           m_ScenePath{};
    std::vector<RTMesh>                   m_Meshes{};
    std::vector<RTMaterial>               m_Materials{};

    VulkanHelpers::Image            m_EnvTexture{};
    VkDescriptorImageInfo           m_EnvTextureDescInfo{};

    VkAccelerationStructureGeometryInstancesDataKHR m_TLASInstances{};
    VkAccelerationStructureGeometryKHR m_TLASGeometry{};
    VkAccelerationStructureBuildGeometryInfoKHR m_BuildInfo{};
    RTAccelerationStructure               m_TLAS{};

    // Shader resources
    std::vector<VkDescriptorBufferInfo>   m_AttribsBufferInfos{};
    std::vector<VkDescriptorBufferInfo>   m_FacesBufferInfos{};
    VulkanHelpers::Buffer                 m_InstancesBuffer{};
    VulkanHelpers::Buffer                 m_MaterialsBuffer{};
    VulkanHelpers::Buffer                 m_ScratchBuffer{};
    std::vector<VkDescriptorImageInfo>    m_TexturesInfos{};
    std::vector<VkDescriptorImageInfo>    m_BumpMapsInfos{};
};