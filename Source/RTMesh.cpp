#include "RTMesh.h"

#include "VKKHR.h"

#include <numeric>

void RTMesh::BuildBLAS(VkDevice device, VkCommandPool cmdPool, VkQueue queue)
{
	VKKHR::LoadPFNs(device);

	VkAccelerationStructureGeometryKHR geometry{};
	VkAccelerationStructureBuildRangeInfoKHR range{};
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

	range.primitiveCount = m_NumFaces;

	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	geometry.geometry.triangles.vertexData = VulkanHelpers::GetBufferDeviceAddressConst(m_PositionsBuffer);
	geometry.geometry.triangles.vertexStride = sizeof(glm::vec3);
	geometry.geometry.triangles.maxVertex = m_NumVertices;
	geometry.geometry.triangles.indexData = VulkanHelpers::GetBufferDeviceAddressConst(m_IndicesBuffer);
	geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;

	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	GetAccelerationStructureBuildSizesKHR(device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		&range.primitiveCount,
		&sizeInfo);

	VulkanHelpers::Buffer scratchBuffer;
	VkResult error = scratchBuffer.Create(sizeInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CHECK_VK_ERROR(error, "scratchBuffer.Create");

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = cmdPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	error = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
	CHECK_VK_ERROR(error, "vkAllocateCommandBuffers");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

	// Build BLASes

	m_BLAS.buffer.Create(sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.buffer = m_BLAS.buffer.GetBuffer();

	error = CreateAccelerationStructureKHR(device, &createInfo, nullptr, &m_BLAS.accelerationStructure);
	CHECK_VK_ERROR(error, "vkCreateAccelerationStructureKHR");

	VkAccelerationStructureBuildGeometryInfoKHR& buildInfos = buildInfo;
	buildInfos.scratchData = VulkanHelpers::GetBufferDeviceAddress(scratchBuffer);
	buildInfos.srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfos.dstAccelerationStructure = m_BLAS.accelerationStructure;

	const VkAccelerationStructureBuildRangeInfoKHR* ranges[1] = { &range };

	CmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfos, ranges);

	// Guard our scratch buffer
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	error = vkQueueWaitIdle(queue);
	CHECK_VK_ERROR(error, "vkQueueWaitIdle");
	vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);

	// Get handle

	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = m_BLAS.accelerationStructure;
	m_BLAS.handle = GetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

void RTMesh::SetPositionToGeometricCentre()
{
	glm::vec3* positions = reinterpret_cast<glm::vec3*>(GetPositionsBuffer().Map());

	position = std::accumulate(positions, positions + m_NumVertices, glm::vec3(0.0f)) / static_cast<float>(m_NumVertices);

	for (int i = 0; i < m_NumVertices; i++)
	{
		positions[i] -= position;
	}

	GetPositionsBuffer().Unmap();
}

VkTransformMatrixKHR RTMesh::GetTransform()
{
	glm::vec3 rot = glm::radians(rotation);

	float t = rot.x;
	glm::mat3 pitch =
	{
		 1.0f,  0.0f,	 0.0f,
		 0.0f,  cos(t), -sin(t),
		 0.0f,  sin(t),  cos(t),
	};

	t = rot.y;
	glm::mat3 yaw =
	{
		 cos(t),  0.0f,  sin(t),
		 0.0f,    1.0f,  0.0f,
		-sin(t),  0.0f,  cos(t),
	};

	t = rot.z;
	glm::mat3 roll =
	{
		 cos(t), -sin(t),  0.0f,
		 sin(t),  cos(t),  0.0f,
		 0.0f,    0.0f,    1.0f,
	};

	rotationMatrix = pitch * yaw * roll;

	VkTransformMatrixKHR transform =
	{
		rotationMatrix[0][0] * scale.x, rotationMatrix[0][1], rotationMatrix[0][2], position.x,
		rotationMatrix[1][0], rotationMatrix[1][1] * scale.y, rotationMatrix[1][2], position.y,
		rotationMatrix[2][0], rotationMatrix[2][1], rotationMatrix[2][2] * scale.z, position.z,
	};

	return transform;
}

void RTMesh::CreateBuffers(uint32_t positions, uint32_t indices, uint32_t faces, uint32_t attribs)
{
	VkResult error = m_PositionsBuffer.Create(positions, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "mesh.positions.Create");

	error = m_IndicesBuffer.Create(indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "mesh.indices.Create");

	error = m_FacesBuffer.Create(faces, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "mesh.faces.Create");

	error = m_AttribsBuffer.Create(attribs, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "mesh.attribs.Create");
}