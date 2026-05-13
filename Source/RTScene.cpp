#include "RTScene.h"

#include "shared_with_shaders.h"
#include "VKKHR.h"

#include "tiny_obj_loader.h"



static const std::filesystem::path s_DefaultTex = "_data/tex/default.jpg";



static void ComputeTangent(VertexAttribute* attribs, glm::vec3* positions, size_t numVerts, uint32_t* indices, size_t numFaces)
{
	for (int i = 0; i < numVerts; i++)
	{
		attribs[i].tangent = glm::vec4(0.0f);
	}

	for (int face = 0; face < numFaces; face++)
	{
		VertexAttribute& v0 = attribs[indices[3 * face + 0]];
		VertexAttribute& v1 = attribs[indices[3 * face + 1]];
		VertexAttribute& v2 = attribs[indices[3 * face + 2]];

		const vec3 e1 = positions[indices[3 * face + 1]] - positions[indices[3 * face + 0]];
		const vec3 e2 = positions[indices[3 * face + 2]] - positions[indices[3 * face + 0]];

		const vec2 dUV1 = v1.uv - v0.uv;
		const vec2 dUV2 = v2.uv - v0.uv;

		const float r = 1.0f / (dUV1.x * dUV2.y - dUV1.y * dUV2.x);

		const vec3 tangent = glm::normalize((e1 * dUV2.y - e2 * dUV1.y) * r);
		const vec3 bitangent = glm::normalize((e2 * dUV1.x - e1 * dUV2.x) * r);

		for (int i = 0; i < 3; i++)
		{
			const float handedness = glm::sign(glm::dot(glm::cross(attribs[indices[3 * face + i]].normal, tangent), bitangent));
			attribs[indices[3 * face + i]].tangent += tangent * handedness;
		}
	}

	for (int i = 0; i < numVerts; i++)
	{
		attribs[i].tangent = glm::normalize(attribs[i].tangent);
	}
}

void RTScene::Destroy(VkDevice device)
{
	for (RTMesh& mesh : m_Meshes)
	{
		DestroyAccelerationStructureKHR(device, mesh.GetBLAS().accelerationStructure, nullptr);
	}

	m_Meshes.clear();
	m_Materials.clear();

	if (m_TLAS.accelerationStructure)
	{
		DestroyAccelerationStructureKHR(device, m_TLAS.accelerationStructure, nullptr);
		m_TLAS.accelerationStructure = VK_NULL_HANDLE;
	}
}

void RTScene::Load(const std::filesystem::path& scenePath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	std::filesystem::path baseDir = scenePath.parent_path();

	const bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, scenePath.string().c_str(), baseDir.string().c_str(), true);
	if (result)
	{
		m_Meshes.resize(shapes.size());
		m_Materials.resize(materials.size());

		VkResult error = m_MaterialsBuffer.Create(materials.size() * sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		CHECK_VK_ERROR(error, "materialsBuffer.Create");

		for (size_t meshIdx = 0; meshIdx < shapes.size(); meshIdx++)
		{
			RTMesh& mesh = m_Meshes[meshIdx];
			const tinyobj::shape_t& shape = shapes[meshIdx];

			mesh.name = shape.name;

			const size_t numFaces = shape.mesh.num_face_vertices.size();
			const size_t numVertices = numFaces * 3;

			mesh.SetNumVertices(numVertices);
			mesh.SetNumFaces(numFaces);

			const size_t positionsBufferSize = numVertices * sizeof(vec3);
			const size_t indicesBufferSize = numFaces * 3 * sizeof(uint32_t);
			const size_t facesBufferSize = numFaces * sizeof(FaceAttribute);
			const size_t attribsBufferSize = numVertices * sizeof(VertexAttribute);

			mesh.CreateBuffers(positionsBufferSize, indicesBufferSize, facesBufferSize, attribsBufferSize);

			vec3* positions = reinterpret_cast<vec3*>(mesh.GetPositionsBuffer().Map());
			uint32_t* indices = reinterpret_cast<uint32_t*>(mesh.GetIndicesBuffer().Map());
			FaceAttribute* faces = reinterpret_cast<FaceAttribute*>(mesh.GetFacesBuffer().Map());
			VertexAttribute* attribs = reinterpret_cast<VertexAttribute*>(mesh.GetAttribsBuffer().Map());

			size_t vIdx = 0;
			for (size_t face = 0; face < numFaces; face++)
			{
				assert(shape.mesh.num_face_vertices[face] == 3);

				for (size_t j = 0; j < 3; j++, vIdx++)
				{
					const tinyobj::index_t& i = shape.mesh.indices[vIdx];

					positions[vIdx] = *reinterpret_cast<vec3*>(&attrib.vertices[3 * i.vertex_index]);
					attribs[vIdx].normal = *reinterpret_cast<vec3*>(&attrib.normals[3 * i.normal_index]);
					attribs[vIdx].uv = *reinterpret_cast<vec2*>(&attrib.texcoords[2 * i.texcoord_index]);
				}

				const uint32_t a = static_cast<uint32_t>(3 * face + 0);
				const uint32_t b = static_cast<uint32_t>(3 * face + 1);
				const uint32_t c = static_cast<uint32_t>(3 * face + 2);

				indices[a] = a;
				indices[b] = b;
				indices[c] = c;

				faces[face].face = uvec3(a, b, c);
				faces[face].matID = shape.mesh.material_ids[face];
			}

			ComputeTangent(attribs, positions, numVertices, indices, numFaces);

			mesh.SetPositionToGeometricCentre();

			mesh.GetIndicesBuffer().Unmap();
			mesh.GetFacesBuffer().Unmap();
			mesh.GetAttribsBuffer().Unmap();
			mesh.GetPositionsBuffer().Unmap();
		}

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		Material* materialsBuffer = reinterpret_cast<Material*>(m_MaterialsBuffer.Map());

		for (size_t i = 0; i < materials.size(); i++)
		{
			const tinyobj::material_t& srcMat = materials[i];
			RTMaterial& dstMat = m_Materials[i];

			dstMat.name = srcMat.name;

			vec3 color(1.0f);
			float bumpStrength = 100.0f;

			if (!srcMat.diffuse_texname.empty())
			{
				if (dstMat.texture.Load(srcMat.diffuse_texname.c_str()))
				{
					dstMat.texture.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, dstMat.texture.GetFormat(), subresourceRange);
					dstMat.texture.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
				}

				std::filesystem::path filePath = srcMat.diffuse_texname;
				std::filesystem::path baseDir = filePath.parent_path();
				std::filesystem::path bumpMapPath = baseDir / ("bump_" + filePath.filename().string());

				if (dstMat.normalMap.Load(bumpMapPath.string().c_str()))
				{
					dstMat.normalMap.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, dstMat.normalMap.GetFormat(), subresourceRange);
					dstMat.normalMap.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
				}

				else if(dstMat.normalMap.Load(s_DefaultTex.string().c_str()))
				{
					dstMat.normalMap.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, dstMat.normalMap.GetFormat(), subresourceRange);
					dstMat.normalMap.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

					bumpStrength = 0.0f;
				}
			}

			else
			{
				if (dstMat.texture.Load(s_DefaultTex.string().c_str()))
				{
					dstMat.texture.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, dstMat.texture.GetFormat(), subresourceRange);
					dstMat.texture.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
				}

				if (dstMat.normalMap.Load(s_DefaultTex.string().c_str()))
				{
					dstMat.normalMap.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, dstMat.normalMap.GetFormat(), subresourceRange);
					dstMat.normalMap.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

					bumpStrength = 0.0f;
				}

				memcpy(&color, srcMat.diffuse, sizeof(vec3));
			}

			// Set material buffer

			materialsBuffer[i].roughness = srcMat.roughness;
			materialsBuffer[i].metallic = srcMat.metallic;
			materialsBuffer[i].smoothness = srcMat.shininess;
			materialsBuffer[i].transmittance = 1.0f - srcMat.dissolve;
			materialsBuffer[i].ior = srcMat.ior;
			materialsBuffer[i].absorptionStrength = 1.0f;
			materialsBuffer[i].bumpStrength = bumpStrength;
			materialsBuffer[i].baseReflectance = vec3(0.04f);
			materialsBuffer[i].diffuseColor = color;
			materialsBuffer[i].specularColor = color;
			materialsBuffer[i].absorptionColor = vec3(1.0f) - color;
			memcpy(&materialsBuffer[i].emission, srcMat.emission, sizeof(vec3));
		}

		m_MaterialsBuffer.Unmap();
	}

	// Prepare shader resources infos
	const size_t numMeshes = m_Meshes.size();
	const size_t numMaterials = m_Materials.size();

	m_AttribsBufferInfos.resize(numMeshes);
	m_FacesBufferInfos.resize(numMeshes);
	for (size_t i = 0; i < numMeshes; i++)
	{
		const RTMesh& mesh = m_Meshes[i];
		VkDescriptorBufferInfo& attribsInfo = m_AttribsBufferInfos[i];
		VkDescriptorBufferInfo& facesInfo = m_FacesBufferInfos[i];

		attribsInfo.buffer = mesh.GetAttribsBuffer().GetBuffer();
		attribsInfo.offset = 0;
		attribsInfo.range = mesh.GetAttribsBuffer().GetSize();

		facesInfo.buffer = mesh.GetFacesBuffer().GetBuffer();
		facesInfo.offset = 0;
		facesInfo.range = mesh.GetFacesBuffer().GetSize();
	}

	m_TexturesInfos.resize(numMaterials);
	m_BumpMapsInfos.resize(numMaterials);
	for (size_t i = 0; i < numMaterials; i++)
	{
		const RTMaterial& mat = m_Materials[i];

		VkDescriptorImageInfo& textureInfo = m_TexturesInfos[i];
		textureInfo.sampler = mat.texture.GetSampler();
		textureInfo.imageView = mat.texture.GetImageView();
		textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo& bumplMapInfo = m_BumpMapsInfos[i];
		bumplMapInfo.sampler = mat.normalMap.GetSampler();
		bumplMapInfo.imageView = mat.normalMap.GetImageView();
		bumplMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
}

void RTScene::BuildTLAS(VkDevice device, VkCommandPool cmdPool, VkQueue queue)
{
	VKKHR::LoadPFNs(device);

	const size_t numMeshes = m_Meshes.size();

	// create instances for our meshes

	std::vector<VkAccelerationStructureInstanceKHR> instances(numMeshes, VkAccelerationStructureInstanceKHR{});
	for (size_t i = 0; i < numMeshes; i++)
	{
		RTMesh& mesh = m_Meshes[i];

		mesh.BuildBLAS(device, cmdPool, queue);

		VkAccelerationStructureInstanceKHR& instance = instances[i];
		instance.transform = mesh.GetTransform();
		instance.instanceCustomIndex = static_cast<uint32_t>(i);
		instance.mask = 0xff;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = mesh.GetBLAS().handle;
	}

	VkResult error = m_InstancesBuffer.Create(instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "m_InstancesBuffer.Create");

	if (!m_InstancesBuffer.UploadData(instances.data(), m_InstancesBuffer.GetSize()))
	{
		assert(false && "Failed to upload instances buffer");
	}


	// and here we create out top-level acceleration structure that'll represent our scene
	VkAccelerationStructureGeometryInstancesDataKHR tlasInstancesInfo{};
	tlasInstancesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	tlasInstancesInfo.data = VulkanHelpers::GetBufferDeviceAddressConst(m_InstancesBuffer);

	VkAccelerationStructureGeometryKHR  tlasGeoInfo{};
	tlasGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	tlasGeoInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	tlasGeoInfo.geometry.instances = tlasInstancesInfo;

	m_BuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	m_BuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	m_BuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	m_BuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	m_BuildInfo.geometryCount = 1;
	m_BuildInfo.pGeometries = &tlasGeoInfo;

	const uint32_t numInstances = static_cast<uint32_t>(instances.size());

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	GetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &m_BuildInfo, &numInstances, &sizeInfo);

	m_TLAS.buffer.Create(sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.buffer = m_TLAS.buffer.GetBuffer();

	error = CreateAccelerationStructureKHR(device, &createInfo, nullptr, &m_TLAS.accelerationStructure);
	CHECK_VK_ERROR(error, "vkCreateAccelerationStructureKHR");


	error = m_ScratchBuffer.Create(sizeInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CHECK_VK_ERROR(error, "scratchBuffer.Create");

	m_BuildInfo.scratchData = VulkanHelpers::GetBufferDeviceAddress(m_ScratchBuffer);
	m_BuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	m_BuildInfo.dstAccelerationStructure = m_TLAS.accelerationStructure;

	VkCommandBuffer commandBuffer = VulkanHelpers::BeginSingleTimeCommandBuffer();

	VkAccelerationStructureBuildRangeInfoKHR range{};
	range.primitiveCount = numInstances;

	const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &range };

	CmdBuildAccelerationStructuresKHR(commandBuffer, 1, &m_BuildInfo, ranges);

	VulkanHelpers::EndSingleTimeCommandBuffer(commandBuffer);

	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = m_TLAS.accelerationStructure;
	m_TLAS.handle = GetAccelerationStructureDeviceAddressKHR(device, &addressInfo);

	// Set build info to update mode after building tlas to get it ready for tlas updates

	m_TLASInstances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	m_TLASInstances.data = VulkanHelpers::GetBufferDeviceAddressConst(m_InstancesBuffer);

	m_TLASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	m_TLASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	m_TLASGeometry.geometry.instances = m_TLASInstances;

	m_BuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	m_BuildInfo.pGeometries = &m_TLASGeometry;
}

void RTScene::UpdateTLAS(VkCommandBuffer commandBuffer)
{
	uint32_t numInstances = m_Meshes.size();

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	GetAccelerationStructureBuildSizesKHR(VKKHR::s_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &m_BuildInfo, &numInstances, &sizeInfo);

	VkResult error = m_ScratchBuffer.Create(sizeInfo.updateScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CHECK_VK_ERROR(error, "scratchBuffer.Create");

	m_BuildInfo.scratchData = VulkanHelpers::GetBufferDeviceAddress(m_ScratchBuffer);
	m_BuildInfo.srcAccelerationStructure = m_TLAS.accelerationStructure;
	m_BuildInfo.dstAccelerationStructure = m_TLAS.accelerationStructure;

	VkAccelerationStructureBuildRangeInfoKHR range{};
	range.primitiveCount = numInstances;

	const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &range };

	CmdBuildAccelerationStructuresKHR(commandBuffer, 1, &m_BuildInfo, ranges);
}