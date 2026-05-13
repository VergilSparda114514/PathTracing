#pragma once

#include "framework/VulkanHelpers.h"

#include "RTAccelerationStructure.h"

class RTMesh
{
public:
    void BuildBLAS(VkDevice device, VkCommandPool cmdPool, VkQueue queue);
    VkTransformMatrixKHR GetTransform();

    void SetNumVertices(uint32_t numVertices) { m_NumVertices = numVertices; }
    void SetNumFaces(uint32_t numFaces) { m_NumFaces = numFaces; }
    void SetPositionToGeometricCentre();

    void CreateBuffers(uint32_t positions, uint32_t indices, uint32_t faces, uint32_t attribs);

    const VulkanHelpers::Buffer& GetPositionsBuffer() const { return m_PositionsBuffer; }
    const VulkanHelpers::Buffer& GetAttribsBuffer() const { return m_AttribsBuffer; }
    const VulkanHelpers::Buffer& GetIndicesBuffer() const { return m_IndicesBuffer; }
    const VulkanHelpers::Buffer& GetFacesBuffer() const { return m_FacesBuffer; }

    const RTAccelerationStructure& GetBLAS() const { return m_BLAS; }
public:
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale{ 1.0f };
    glm::mat3 rotationMatrix{ 1.0f };
private:
    uint32_t                    m_NumVertices;
    uint32_t                    m_NumFaces;

    VulkanHelpers::Buffer       m_PositionsBuffer;
    VulkanHelpers::Buffer       m_AttribsBuffer;
    VulkanHelpers::Buffer       m_IndicesBuffer;
    VulkanHelpers::Buffer       m_FacesBuffer;

    RTAccelerationStructure     m_BLAS;
};