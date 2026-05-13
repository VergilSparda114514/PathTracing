#include "RTXApp.h"

#include "shared_with_shaders.h"
#include "VKKHR.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>

static const std::filesystem::path s_ShadersFolder = "_data/shaders/";
static const std::filesystem::path s_ScenesFolder = "_data/scenes/";
static const std::filesystem::path s_EnvsFolder = "_data/envs/";

static uint32_t NextPowerOf2(uint32_t n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

void RTXApp::InitSettings()
{
	m_Settings.name = "Ray Tracing";
	m_Settings.fullscreen = false;
	m_Settings.enableValidation = true;
	m_Settings.enableVSync = true;
	m_Settings.supportRaytracing = true;
	m_Settings.supportDescriptorIndexing = true;
	m_Settings.resolutionX = 1980;
	m_Settings.resolutionY = 1080;
	m_Settings.supportDocking = false;
}

void RTXApp::InitApp()
{
	VKKHR::LoadPFNs(m_Device);
	m_Scene.Load(s_ScenesFolder / "sponzas/dabrovic_sponza.obj");
	CreateScene();
	CreateBuffers();
	CreateResultImage();
	CreateRTDescriptorSetsLayouts();
	CreateRTPipelineAndSBT();
	UpdateRTDescriptorSets();
	CreateComputeDescriptorSetsLayouts();
	CreateComputePipeline();
	UpdateComputeDescriptorSets();
}

void RTXApp::FreeResources()
{
	m_Scene.Destroy(m_Device);

	if (m_RTDescriptorPool)
	{
		vkDestroyDescriptorPool(m_Device, m_RTDescriptorPool, nullptr);
		m_RTDescriptorPool = VK_NULL_HANDLE;
	}

	m_SBT.Destroy();

	if (m_RTPipeline)
	{
		vkDestroyPipeline(m_Device, m_RTPipeline, nullptr);
		m_RTPipeline = VK_NULL_HANDLE;
	}

	if (m_RTPipelineLayout)
	{
		vkDestroyPipelineLayout(m_Device, m_RTPipelineLayout, nullptr);
		m_RTPipelineLayout = VK_NULL_HANDLE;
	}

	for (VkDescriptorSetLayout& dsl : m_RTDescriptorSetsLayouts)
	{
		vkDestroyDescriptorSetLayout(m_Device, dsl, nullptr);
	}
}

void RTXApp::FillCommandBuffer(VkCommandBuffer commandBuffer, size_t)
{
	vkCmdBindPipeline(commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_RTPipeline);

	vkCmdBindDescriptorSets(commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_RTPipelineLayout, 0,
		static_cast<uint32_t>(m_RTDescriptorSets.size()), m_RTDescriptorSets.data(),
		0, nullptr);

	VkStridedDeviceAddressRegionKHR raygenRegion =
	{
		m_SBT.GetSBTAddress() + m_SBT.GetRaygenOffset(),
		m_SBT.GetGroupsStride(),
		m_SBT.GetRaygenSize(),
	};

	VkStridedDeviceAddressRegionKHR missRegion =
	{
		m_SBT.GetSBTAddress() + m_SBT.GetMissGroupsOffset(),
		m_SBT.GetGroupsStride(),
		m_SBT.GetMissGroupsSize(),
	};

	VkStridedDeviceAddressRegionKHR hitRegion =
	{
		m_SBT.GetSBTAddress() + m_SBT.GetHitGroupsOffset(),
		m_SBT.GetGroupsStride(),
		m_SBT.GetHitGroupsSize(),
	};

	VkStridedDeviceAddressRegionKHR callableRegion{};

	m_Scene.UpdateTLAS(commandBuffer);

	CmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion, m_Settings.resolutionX, m_Settings.resolutionY, 1u);

	// Compute pass to do postprocessing and then copy the result image to the swapchain image

	UpdatePingPongDescriptorSets(&m_PingDescInfo, &m_PongDescInfo);

	vkCmdBindDescriptorSets(commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		m_ComputePipelineLayout, 0,
		static_cast<uint32_t>(m_ComputeDescriptorSets.size()),
		m_ComputeDescriptorSets.data(),
		0, nullptr);

	uint32_t width = (m_Settings.resolutionX + 9) / 10;
	uint32_t height = (m_Settings.resolutionY + 9) / 10;

#if 0

#endif

	m_CompositePass.Dispatch(commandBuffer, { width, height, 1u });
}

void RTXApp::OnUIRender(float deltaTime)
{
	{
		ImGui::Begin("Settings");

		ImGui::Text("%.1f FPS (%.3fms)", 1.0f / deltaTime, 1000.0f * deltaTime);

		CameraParams* cameraParams = reinterpret_cast<CameraParams*>(m_Camera.GetBuffer().Map());
		LightingParams* lightingParams = reinterpret_cast<LightingParams*>(m_LightingBuffer.Map());
		PostProcessParams* ppParams = reinterpret_cast<PostProcessParams*>(m_PostProcessBuffer.Map());

		static bool tmpAcc = true;

		ImGui::Text("Ray");

		ImGui::SliderInt("Rays per Pixel", &lightingParams->numSamples, 1, 3);
		ImGui::SliderInt("Max Bounces", &lightingParams->maxRecursion, 1, 7);
		ImGui::Checkbox("Temporal Accumulation", &tmpAcc);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::Text("Depth of Field");

		ImGui::Checkbox("Enable DOF", &cameraParams->enableDOF);
		ImGui::Checkbox("Auto Focus", &cameraParams->autoFocus);
		ImGui::SliderFloat("Aperature Size", &cameraParams->apertureSize, 0.0f, 1.0f);
		ImGui::SliderFloat("Focus Speed", &cameraParams->focusSpeed, 0.0f, 10.0f);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::Text("Post Processing");

		const char* items[] =
		{
			"None",
			"Neutral",
			"ACES"
		};

		ImGui::DragFloat("Bloom Threshold", &ppParams->bloomThreshold, 0.1f, 0.0f, 0.0f, "%.1f");
		ImGui::DragFloat("Bloom Strength", &ppParams->bloomStrength, 0.1f, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tone Mapping", &ppParams->toneMappingMode, items, IM_ARRAYSIZE(items));
		ImGui::SliderFloat("Exposure", &ppParams->exposure, 0.0f, 10.0f);
		ImGui::SliderFloat("Contrast", &ppParams->contrast, 0.0f, 10.0f);
		ImGui::SliderFloat("Saturation", &ppParams->saturation, 0.0f, 10.0f);

		if (!tmpAcc)
		{
			m_AccumulatedFrame = 0;
		}

		lightingParams->deltaTime = deltaTime;
		lightingParams->frame++;
		lightingParams->accumulationFrame = m_AccumulatedFrame++;

		m_PostProcessBuffer.Unmap();
		m_LightingBuffer.Unmap();
		m_Camera.GetBuffer().Unmap();

		ImGui::End();
	}

	{
		ImGui::Begin("Objects");

		for (RTMesh& mesh : m_Scene.GetMeshes())
		{
			ImGui::PushID(&mesh.name);

			ImGui::Text(std::format("{}", mesh.name).c_str());

			ImGui::DragFloat3("Position", glm::value_ptr(mesh.position), 0.01f);
			ImGui::DragFloat3("Rotation", glm::value_ptr(mesh.rotation), 0.1f);
			ImGui::DragFloat3("Scale", glm::value_ptr(mesh.scale), 0.01f);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PopID();
		}

		ImGui::End();
	}

	{
		ImGui::Begin("Materials");

		int numMaterials = m_Scene.GetMaterials().size();
		Material* materials = reinterpret_cast<Material*>(m_Scene.GetMaterialsBuffer().Map());

		for (int i = 0; i < numMaterials; i++)
		{
			Material& material = materials[i];

			ImGui::PushID(&material);

			ImGui::Text(std::format("{}", m_Scene.GetMaterials()[i].name).c_str());

			ImGui::SliderFloat("Rougness", &material.roughness, 0.0f, 1.0f);
			ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
			ImGui::SliderFloat("Smoothness", &material.smoothness, 0.0f, 1.0f);
			ImGui::SliderFloat("Transmittance", &material.transmittance, 0.0f, 1.0f);
			ImGui::SliderFloat("IOR", &material.ior, 0.0f, 2.0f);
			ImGui::DragFloat("Absorption Strength", &material.absorptionStrength, 0.01f, 0.0f, std::numeric_limits<float>::max());
			ImGui::DragFloat("Bump Strength", &material.bumpStrength);
			ImGui::ColorEdit3("Base Reflectance", glm::value_ptr(material.baseReflectance));
			ImGui::ColorEdit3("Diffuse Color", glm::value_ptr(material.diffuseColor));
			ImGui::ColorEdit3("Specular Color", glm::value_ptr(material.specularColor));
			ImGui::ColorEdit3("Absorption Color", glm::value_ptr(material.absorptionColor));
			ImGui::ColorEdit3("Emission", glm::value_ptr(material.emission), ImGuiColorEditFlags_::ImGuiColorEditFlags_HDR);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PopID();
		}

		m_Scene.GetMaterialsBuffer().Unmap();

		ImGui::End();
	}
}

void RTXApp::OnUpdate(size_t, float deltaTime)
{
	const std::vector<RTMesh>& meshes = m_Scene.GetMeshes();
	const uint32_t numInstances = m_Scene.GetMeshes().size();

	VkAccelerationStructureInstanceKHR* instances = static_cast<VkAccelerationStructureInstanceKHR*>(m_Scene.GetInstancesBuffer().Map());

	for (int i = 0; i < numInstances; i++)
	{
		instances[i].transform = m_Scene.GetMeshes()[i].GetTransform();
	}

	m_Scene.GetInstancesBuffer().Unmap();

	if (m_Camera.OnUpdate(deltaTime))
	{
		m_AccumulatedFrame = 0;
	}
}

void RTXApp::OnResize()
{
	m_ResultImage.Destroy();
	CreateResultImage();

	UpdateRTDescriptorSets();
	UpdateComputeDescriptorSets();

	m_Camera.OnResize(m_Settings.resolutionX, m_Settings.resolutionY);
}



void RTXApp::CreateScene()
{
	m_Scene.BuildTLAS(m_Device, m_CommandPool, m_GraphicsQueue);

	m_EnvTexture.Load((s_EnvsFolder / "studio_garden_2k.jpg").string().c_str());

	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	m_EnvTexture.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, m_EnvTexture.GetFormat(), subresourceRange);
	m_EnvTexture.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

	m_EnvTextureDescInfo.sampler = m_EnvTexture.GetSampler();
	m_EnvTextureDescInfo.imageView = m_EnvTexture.GetImageView();
	m_EnvTextureDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void RTXApp::CreateBuffers()
{
	m_Camera.CreateBuffer();

	{
		VkResult error = m_LightingBuffer.Create(sizeof(LightingParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		CHECK_VK_ERROR(error, "mLightingBuffer.Create");

		m_LightingBuffer.UploadData(new LightingParams(), sizeof(LightingParams));
	}

	{
		VkResult error = m_PostProcessBuffer.Create(sizeof(PostProcessParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		CHECK_VK_ERROR(error, "mPostProcessBuffer.Create");

		m_PostProcessBuffer.UploadData(new PostProcessParams(), sizeof(PostProcessParams));
	}
}

void RTXApp::CreateResultImage()
{
	VkExtent3D extent = { m_Settings.resolutionX, m_Settings.resolutionY, 1 };

	m_ResultImage.CreateRGBA32(extent);

	extent.width = NextPowerOf2(extent.width);
	extent.height = NextPowerOf2(extent.height);

	m_PingImage.CreateRGBA32(extent);
	m_PongImage.CreateRGBA32(extent);

	m_PingDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	m_PingDescInfo.imageView = m_PingImage.GetImageView();
	m_PingDescInfo.sampler = m_PingImage.GetSampler();

	m_PongDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	m_PongDescInfo.imageView = m_PongImage.GetImageView();
	m_PongDescInfo.sampler = m_PongImage.GetSampler();

	uint32_t size = m_Settings.resolutionX * m_Settings.resolutionY;

	m_CurrResovoirBuffer.Create(sizeof(Reservoir) * size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_PrevResovoirBuffer.Create(sizeof(Reservoir) * size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void RTXApp::CreateRTDescriptorSetsLayouts()
{
	const uint32_t numMeshes = static_cast<uint32_t>(m_Scene.GetMeshes().size());
	const uint32_t numMaterials = static_cast<uint32_t>(m_Scene.GetMaterials().size());

	// set 0:
	// binding 0  ->  AS
	// binding 1  ->  Environment
	// binding 2  ->  Camera data
	// binding 3  ->  output image

	VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
	accelerationStructureLayoutBinding.binding = SCENE_AS_BINDING;
	accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureLayoutBinding.descriptorCount = 1;
	accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding envBinding{};
	envBinding.binding = SCENE_ENV_BINDING;
	envBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	envBinding.descriptorCount = 1;
	envBinding.stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;
	envBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding litDataBufferBinding{};
	litDataBufferBinding.binding = SCENE_LIT_BINDING;
	litDataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	litDataBufferBinding.descriptorCount = 1;
	litDataBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	litDataBufferBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding camDataBufferBinding{};
	camDataBufferBinding.binding = SCENE_CAM_BINDING;
	camDataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	camDataBufferBinding.descriptorCount = 1;
	camDataBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	camDataBufferBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding resultImageLayoutBinding{};
	resultImageLayoutBinding.binding = SCENE_IMG_BINDING;
	resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	resultImageLayoutBinding.descriptorCount = 1;
	resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	resultImageLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings =
	{
		accelerationStructureLayoutBinding,
		envBinding,
		litDataBufferBinding,
		camDataBufferBinding,
		resultImageLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo set0LayoutInfo{};
	set0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set0LayoutInfo.pNext = nullptr;
	set0LayoutInfo.flags = 0;
	set0LayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	set0LayoutInfo.pBindings = bindings.data();

	VkResult error = vkCreateDescriptorSetLayout(m_Device, &set0LayoutInfo, nullptr, &m_RTDescriptorSetsLayouts[SCENE_SET]);
	CHECK_VK_ERROR(error, "vkCreateDescriptorSetLayout");

	// set layout info for the rest of the sets

	const VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.pNext = nullptr;
	bindingFlags.pBindingFlags = &flag;
	bindingFlags.bindingCount = 1;

	VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
	setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutInfo.pNext = &bindingFlags;
	setLayoutInfo.flags = 0;
	setLayoutInfo.bindingCount = 1;

	// set 1:
	// binding 0 .. N  ->  vertex attributes for our meshes  (N = num meshes)

	VkDescriptorSetLayoutBinding ssboBinding{};
	ssboBinding.binding = 0;
	ssboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ssboBinding.descriptorCount = numMeshes;
	ssboBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	ssboBinding.pImmutableSamplers = nullptr;

	setLayoutInfo.pBindings = &ssboBinding;

	error = vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_RTDescriptorSetsLayouts[ATRIB_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// set 2:
	// binding 0 .. N  ->  faces info (indices) for our meshes  (N = num meshes)

	error = vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_RTDescriptorSetsLayouts[FACES_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// set 3:
	// binding 0 -> materials
	// binding 1 .. N + 1  ->  textures (N = num materials)

	VkDescriptorSetLayoutBinding materialBinding{};
	materialBinding.binding = 0;
	materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialBinding.descriptorCount = 1;
	materialBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	materialBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding textureBinding{};
	textureBinding.binding = 1;
	textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBinding.descriptorCount = numMaterials;
	textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	textureBinding.pImmutableSamplers = nullptr;

	bindings =
	{
		materialBinding,
		textureBinding,
	};

	setLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	setLayoutInfo.pBindings = bindings.data();

	error = vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_RTDescriptorSetsLayouts[MAT_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// set 4:
	// binding 0 .. N  ->  bump maps (N = num materials)

	textureBinding.binding = 0;

	setLayoutInfo.bindingCount = 1;
	setLayoutInfo.pBindings = &textureBinding;

	error = vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_RTDescriptorSetsLayouts[NORM_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// set 5:
	// binding 0  ->  current frame reservoirs
	// binding 1  ->  previous frame reservoirs

	VkDescriptorSetLayoutBinding currReservoirBinding{};
	currReservoirBinding.binding = 0;
	currReservoirBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	currReservoirBinding.descriptorCount = 1;
	currReservoirBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	currReservoirBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding prevReservoirBinding{};
	prevReservoirBinding.binding = 1;
	prevReservoirBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	prevReservoirBinding.descriptorCount = 1;
	prevReservoirBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	prevReservoirBinding.pImmutableSamplers = nullptr;

	bindings =
	{
		currReservoirBinding,
		prevReservoirBinding,
	};

	setLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	setLayoutInfo.pBindings = bindings.data();

	error = vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_RTDescriptorSetsLayouts[RESO_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");
}

void RTXApp::UpdateRTDescriptorSets()
{
	const uint32_t numMeshes = static_cast<uint32_t>(m_Scene.GetMeshes().size());
	const uint32_t numMaterials = static_cast<uint32_t>(m_Scene.GetMaterials().size());

	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },       // TLAS
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },           // Environment texture
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },                   // Lighting data
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },                   // Camera data
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },                    // Output image

		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numMeshes * 3 },
		// Vertex attribs for each mesh
		// Faces buffer for each mesh
		// bumps buffer for each mesh

		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },		// Materials
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numMaterials }, // Texture for each material
		
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numMaterials }, // bump map for each material
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = NUM_SETS;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

	VkResult error = vkCreateDescriptorPool(m_Device, &descriptorPoolCreateInfo, nullptr, &m_RTDescriptorPool);
	CHECK_VK_ERROR(error, "vkCreateDescriptorPool");

	std::array<uint32_t, NUM_SETS> variableDescriptorCounts =
	{
		1,
		numMeshes,      // vertex attribs for each mesh
		numMeshes,      // faces buffer for each mesh
		numMaterials + 1,   // materials
		numMaterials,   // bump maps
		2,
	};

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo{};
	variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableDescriptorCountInfo.pNext = nullptr;
	variableDescriptorCountInfo.descriptorSetCount = NUM_SETS;
	variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data(); // actual number of descriptors

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
	descriptorSetAllocateInfo.descriptorPool = m_RTDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = NUM_SETS;
	descriptorSetAllocateInfo.pSetLayouts = m_RTDescriptorSetsLayouts.data();

	error = vkAllocateDescriptorSets(m_Device, &descriptorSetAllocateInfo, m_RTDescriptorSets.data());
	CHECK_VK_ERROR(error, "vkAllocateDescriptorSets");

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet accelerationStructureWrite{};
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	{
		VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
		descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		descriptorAccelerationStructureInfo.pNext = nullptr;
		descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
		descriptorAccelerationStructureInfo.pAccelerationStructures = &m_Scene.GetTLAS().accelerationStructure;

		accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
	}
	accelerationStructureWrite.dstSet = m_RTDescriptorSets[SCENE_SET];
	accelerationStructureWrite.dstBinding = SCENE_AS_BINDING;
	accelerationStructureWrite.dstArrayElement = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureWrite.pImageInfo = nullptr;
	accelerationStructureWrite.pBufferInfo = nullptr;
	accelerationStructureWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet envTexturesWrite{};
	envTexturesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	envTexturesWrite.pNext = nullptr;
	envTexturesWrite.dstSet = m_RTDescriptorSets[SCENE_SET];
	envTexturesWrite.dstBinding = SCENE_ENV_BINDING;
	envTexturesWrite.dstArrayElement = 0;
	envTexturesWrite.descriptorCount = 1;
	envTexturesWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	envTexturesWrite.pImageInfo = &m_EnvTextureDescInfo;
	envTexturesWrite.pBufferInfo = nullptr;
	envTexturesWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet litDataBufferWrite{};
	litDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	litDataBufferWrite.pNext = nullptr;
	litDataBufferWrite.dstSet = m_RTDescriptorSets[SCENE_SET];
	litDataBufferWrite.dstBinding = SCENE_LIT_BINDING;
	litDataBufferWrite.dstArrayElement = 0;
	litDataBufferWrite.descriptorCount = 1;
	litDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	litDataBufferWrite.pImageInfo = nullptr;
	{
		VkDescriptorBufferInfo litDataBufferInfo{};
		litDataBufferInfo.buffer = m_LightingBuffer.GetBuffer();
		litDataBufferInfo.offset = 0;
		litDataBufferInfo.range = m_LightingBuffer.GetSize();

		litDataBufferWrite.pBufferInfo = &litDataBufferInfo;
	}
	litDataBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet camDataBufferWrite{};
	camDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	camDataBufferWrite.pNext = nullptr;
	camDataBufferWrite.dstSet = m_RTDescriptorSets[SCENE_SET];
	camDataBufferWrite.dstBinding = SCENE_CAM_BINDING;
	camDataBufferWrite.dstArrayElement = 0;
	camDataBufferWrite.descriptorCount = 1;
	camDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	camDataBufferWrite.pImageInfo = nullptr;
	{
		VkDescriptorBufferInfo camDataBufferInfo{};
		camDataBufferInfo.buffer = m_Camera.GetBuffer().GetBuffer();
		camDataBufferInfo.offset = 0;
		camDataBufferInfo.range = m_Camera.GetBuffer().GetSize();

		camDataBufferWrite.pBufferInfo = &camDataBufferInfo;
	}
	camDataBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet resultImageWrite{};
	resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	resultImageWrite.pNext = nullptr;
	resultImageWrite.dstSet = m_RTDescriptorSets[SCENE_SET];
	resultImageWrite.dstBinding = SCENE_IMG_BINDING;
	resultImageWrite.dstArrayElement = 0;
	resultImageWrite.descriptorCount = 1;
	resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	{
		VkDescriptorImageInfo descriptorOutputImageInfo{};
		descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
		descriptorOutputImageInfo.imageView = m_ResultImage.GetImageView();
		descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		resultImageWrite.pImageInfo = &descriptorOutputImageInfo;
	}
	resultImageWrite.pBufferInfo = nullptr;
	resultImageWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet attribsBufferWrite{};
	attribsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	attribsBufferWrite.pNext = nullptr;
	attribsBufferWrite.dstSet = m_RTDescriptorSets[ATRIB_SET];
	attribsBufferWrite.dstBinding = 0;
	attribsBufferWrite.dstArrayElement = 0;
	attribsBufferWrite.descriptorCount = numMeshes;
	attribsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	attribsBufferWrite.pImageInfo = nullptr;
	attribsBufferWrite.pBufferInfo = m_Scene.GetAttribsBufferInfos().data();
	attribsBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet facesBufferWrite{};
	facesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	facesBufferWrite.pNext = nullptr;
	facesBufferWrite.dstSet = m_RTDescriptorSets[FACES_SET];
	facesBufferWrite.dstBinding = 0;
	facesBufferWrite.dstArrayElement = 0;
	facesBufferWrite.descriptorCount = numMeshes;
	facesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	facesBufferWrite.pImageInfo = nullptr;
	facesBufferWrite.pBufferInfo = m_Scene.GetFacesBufferInfos().data();
	facesBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet materialsBufferWrite{};
	materialsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	materialsBufferWrite.pNext = nullptr;
	materialsBufferWrite.dstSet = m_RTDescriptorSets[MAT_SET];
	materialsBufferWrite.dstBinding = 0;
	materialsBufferWrite.dstArrayElement = 0;
	materialsBufferWrite.descriptorCount = 1;
	materialsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsBufferWrite.pImageInfo = nullptr;
	{
		VkDescriptorBufferInfo matDataBufferInfo{};
		matDataBufferInfo.buffer = m_Scene.GetMaterialsBuffer().GetBuffer();
		matDataBufferInfo.offset = 0;
		matDataBufferInfo.range = m_Scene.GetMaterialsBuffer().GetSize();

		materialsBufferWrite.pBufferInfo = &matDataBufferInfo;
	}
	materialsBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet texturesBufferWrite{};
	texturesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesBufferWrite.pNext = nullptr;
	texturesBufferWrite.dstSet = m_RTDescriptorSets[MAT_SET];
	texturesBufferWrite.dstBinding = 1;
	texturesBufferWrite.dstArrayElement = 0;
	texturesBufferWrite.descriptorCount = numMaterials;
	texturesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesBufferWrite.pImageInfo = m_Scene.GetTexturesInfos().data();
	texturesBufferWrite.pBufferInfo = nullptr;
	texturesBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet bumpMapsBufferWrite{};
	bumpMapsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	bumpMapsBufferWrite.pNext = nullptr;
	bumpMapsBufferWrite.dstSet = m_RTDescriptorSets[NORM_SET];
	bumpMapsBufferWrite.dstBinding = 0;
	bumpMapsBufferWrite.dstArrayElement = 0;
	bumpMapsBufferWrite.descriptorCount = numMaterials;
	bumpMapsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bumpMapsBufferWrite.pImageInfo = m_Scene.GetBumpMapsInfos().data();
	bumpMapsBufferWrite.pBufferInfo = nullptr;
	bumpMapsBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet currReservoirBufferWrite{};
	currReservoirBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	currReservoirBufferWrite.pNext = nullptr;
	currReservoirBufferWrite.dstSet = m_RTDescriptorSets[RESO_SET];
	currReservoirBufferWrite.dstBinding = 0;
	currReservoirBufferWrite.dstArrayElement = 0;
	currReservoirBufferWrite.descriptorCount = 1;
	currReservoirBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	currReservoirBufferWrite.pImageInfo = nullptr;
	{
		VkDescriptorBufferInfo resoDataBufferInfo{};
		resoDataBufferInfo.buffer = m_CurrResovoirBuffer.GetBuffer();
		resoDataBufferInfo.offset = 0;
		resoDataBufferInfo.range = m_CurrResovoirBuffer.GetSize();

		currReservoirBufferWrite.pBufferInfo = &resoDataBufferInfo;
	}
	currReservoirBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet prevReservoirBufferWrite{};
	prevReservoirBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	prevReservoirBufferWrite.pNext = nullptr;
	prevReservoirBufferWrite.dstSet = m_RTDescriptorSets[RESO_SET];
	prevReservoirBufferWrite.dstBinding = 1;
	prevReservoirBufferWrite.dstArrayElement = 0;
	prevReservoirBufferWrite.descriptorCount = 1;
	prevReservoirBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	prevReservoirBufferWrite.pImageInfo = nullptr;
	{
		VkDescriptorBufferInfo resoDataBufferInfo{};
		resoDataBufferInfo.buffer = m_PrevResovoirBuffer.GetBuffer();
		resoDataBufferInfo.offset = 0;
		resoDataBufferInfo.range = m_PrevResovoirBuffer.GetSize();

		prevReservoirBufferWrite.pBufferInfo = &resoDataBufferInfo;
	}
	prevReservoirBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	std::vector<VkWriteDescriptorSet> descriptorWrites =
	{
		accelerationStructureWrite,
		envTexturesWrite,
		litDataBufferWrite,
		camDataBufferWrite,
		resultImageWrite,
		//
		attribsBufferWrite,
		//
		facesBufferWrite,
		//
		materialsBufferWrite,
		texturesBufferWrite,
		//
		bumpMapsBufferWrite,
		//
		currReservoirBufferWrite,
		prevReservoirBufferWrite,
	};

	vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);
}

void RTXApp::CreateRTPipelineAndSBT()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(m_RTDescriptorSetsLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = m_RTDescriptorSetsLayouts.data();

	VkResult error = vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_RTPipelineLayout);
	CHECK_VK_ERROR(error, "vkCreatePipelineLayout");

	VulkanHelpers::Shader rayGenShader, rayChitShader, rayMissShader;

	rayGenShader.LoadFromFile((s_ShadersFolder / "ray_gen.bin").string().c_str());
	rayChitShader.LoadFromFile((s_ShadersFolder / "ray_chit.bin").string().c_str());
	rayMissShader.LoadFromFile((s_ShadersFolder / "ray_miss.bin").string().c_str());
	m_SBT.Initialize(1, 1, m_RTProperties.shaderGroupHandleSize, m_RTProperties.shaderGroupBaseAlignment);

	m_SBT.SetRaygenStage(rayGenShader.GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR));
	m_SBT.AddStageToHitGroup({ rayChitShader.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, 0);
	m_SBT.AddStageToMissGroup(rayMissShader.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR), 0);

	VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{};
	rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayPipelineInfo.stageCount = m_SBT.GetNumStages();
	rayPipelineInfo.pStages = m_SBT.GetStages();
	rayPipelineInfo.groupCount = m_SBT.GetNumGroups();
	rayPipelineInfo.pGroups = m_SBT.GetGroups();
	rayPipelineInfo.maxPipelineRayRecursionDepth = 1;
	rayPipelineInfo.layout = m_RTPipelineLayout;

	error = CreateRayTracingPipelinesKHR(m_Device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayPipelineInfo, VK_NULL_HANDLE, &m_RTPipeline);
	CHECK_VK_ERROR(error, "vkCreateRayTracingPipelinesKHR");

	m_SBT.CreateSBT(m_Device, m_RTPipeline);
}

void RTXApp::CreateComputeDescriptorSetsLayouts()
{
	m_ComputeDescriptorSets.resize(2);
	m_ComputeDescriptorSetsLayouts.resize(2);

	// set 0:
	// binding 0  ->  Result Image
	// binding 1  ->  Final Image
	// binding 2  ->  Post Process Buffer

	const VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.pNext = nullptr;
	bindingFlags.pBindingFlags = &flag;
	bindingFlags.bindingCount = 1;

	VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
	setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutInfo.pNext = &bindingFlags;
	setLayoutInfo.flags = 0;

	///////////////////////////////////////////////////////////

	VkDescriptorSetLayoutBinding resultImageLayoutBinding{};
	resultImageLayoutBinding.binding = 0;
	resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	resultImageLayoutBinding.descriptorCount = 1;
	resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	resultImageLayoutBinding.pImmutableSamplers = nullptr;

	///////////////////////////////////////////////////////////

	VkDescriptorSetLayoutBinding finalImageLayoutBinding{};
	finalImageLayoutBinding.binding = 1;
	finalImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	finalImageLayoutBinding.descriptorCount = 1;
	finalImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	finalImageLayoutBinding.pImmutableSamplers = nullptr;

	///////////////////////////////////////////////////////////

	VkDescriptorSetLayoutBinding ppDataBufferBinding{};
	ppDataBufferBinding.binding = 2;
	ppDataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ppDataBufferBinding.descriptorCount = 1;
	ppDataBufferBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	ppDataBufferBinding.pImmutableSamplers = nullptr;

	///////////////////////////////////////////////////////////

	std::vector<VkDescriptorSetLayoutBinding> bindings =
	{
		resultImageLayoutBinding,
		finalImageLayoutBinding,
		ppDataBufferBinding,
	};

	setLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	setLayoutInfo.pBindings = bindings.data();

	VkResult error = vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_ComputeDescriptorSetsLayouts[0]);
	CHECK_VK_ERROR(error, "vkCreateDescriptorSetLayout");

	// set 1:
	// binding 0  ->  Ping Image
	// binding 1  ->  Pong Image

	VkDescriptorSetLayoutBinding pingImageLayoutBinding{};
	pingImageLayoutBinding.binding = 0;
	pingImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pingImageLayoutBinding.descriptorCount = 1;
	pingImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pingImageLayoutBinding.pImmutableSamplers = nullptr;

	///////////////////////////////////////////////////////////

	VkDescriptorSetLayoutBinding pongImageLayoutBinding{};
	pongImageLayoutBinding.binding = 1;
	pongImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pongImageLayoutBinding.descriptorCount = 1;
	pongImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pongImageLayoutBinding.pImmutableSamplers = nullptr;

	///////////////////////////////////////////////////////////

	bindings =
	{
		pingImageLayoutBinding,
		pongImageLayoutBinding,
	};

	setLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	setLayoutInfo.pBindings = bindings.data();

	error = vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_ComputeDescriptorSetsLayouts[1]);
	CHECK_VK_ERROR(error, "vkCreateDescriptorSetLayout");
}

void RTXApp::UpdateComputeDescriptorSets()
{
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },                    // Result and Output image
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },					// Post processing data
		//
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },                    // Ping and Pong image
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(m_ComputeDescriptorSets.size());
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

	VkResult error = vkCreateDescriptorPool(m_Device, &descriptorPoolCreateInfo, nullptr, &m_ComputeDescriptorPool);
	CHECK_VK_ERROR(error, "vkCreateDescriptorPool");

	///////////////////////////////////////////////////////////

	std::vector<uint32_t> variableDescriptorCounts =
	{
		3, // Result & Offscreen image + Post Process Buffer
		2, // Ping & Pong
	};

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo{};
	variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableDescriptorCountInfo.pNext = nullptr;
	variableDescriptorCountInfo.descriptorSetCount = static_cast<uint32_t>(variableDescriptorCounts.size());
	variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data(); // actual number of descriptors

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
	descriptorSetAllocateInfo.descriptorPool = m_ComputeDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(m_ComputeDescriptorSetsLayouts.size());
	descriptorSetAllocateInfo.pSetLayouts = m_ComputeDescriptorSetsLayouts.data();

	error = vkAllocateDescriptorSets(m_Device, &descriptorSetAllocateInfo, m_ComputeDescriptorSets.data());
	CHECK_VK_ERROR(error, "vkAllocateDescriptorSets");

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet resultImageWrite{};
	resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	resultImageWrite.pNext = nullptr;
	resultImageWrite.dstSet = m_ComputeDescriptorSets[0];
	resultImageWrite.dstBinding = 0;
	resultImageWrite.dstArrayElement = 0;
	resultImageWrite.descriptorCount = 1;
	resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	{
		VkDescriptorImageInfo descriptorOutputImageInfo{};
		descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
		descriptorOutputImageInfo.imageView = m_ResultImage.GetImageView();
		descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		resultImageWrite.pImageInfo = &descriptorOutputImageInfo;
	}
	resultImageWrite.pBufferInfo = nullptr;
	resultImageWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet finalImageWrite{};
	finalImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	finalImageWrite.pNext = nullptr;
	finalImageWrite.dstSet = m_ComputeDescriptorSets[0];
	finalImageWrite.dstBinding = 1;
	finalImageWrite.dstArrayElement = 0;
	finalImageWrite.descriptorCount = 1;
	finalImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	{
		VkDescriptorImageInfo descriptorOutputImageInfo{};
		descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
		descriptorOutputImageInfo.imageView = m_OffscreenImage.GetImageView();
		descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		finalImageWrite.pImageInfo = &descriptorOutputImageInfo;
	}
	finalImageWrite.pBufferInfo = nullptr;
	finalImageWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet ppDataBufferWrite{};
	ppDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	ppDataBufferWrite.pNext = nullptr;
	ppDataBufferWrite.dstSet = m_ComputeDescriptorSets[0];
	ppDataBufferWrite.dstBinding = 2;
	ppDataBufferWrite.dstArrayElement = 0;
	ppDataBufferWrite.descriptorCount = 1;
	ppDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ppDataBufferWrite.pImageInfo = nullptr;
	{
		VkDescriptorBufferInfo ppDataBufferInfo{};
		ppDataBufferInfo.buffer = m_PostProcessBuffer.GetBuffer();
		ppDataBufferInfo.offset = 0;
		ppDataBufferInfo.range = m_PostProcessBuffer.GetSize();

		ppDataBufferWrite.pBufferInfo = &ppDataBufferInfo;
	}
	ppDataBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	// UpdatePingPongDescriptorSets(&m_PingDescInfo, &m_PongDescInfo);

	///////////////////////////////////////////////////////////

	std::vector<VkWriteDescriptorSet> descriptorWrites =
	{
		resultImageWrite,
		finalImageWrite,
		ppDataBufferWrite,
	};

	vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);
}

void RTXApp::UpdatePingPongDescriptorSets(VkDescriptorImageInfo* ping, VkDescriptorImageInfo* pong)
{
	VkWriteDescriptorSet pingImageWrite{};
	pingImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	pingImageWrite.pNext = nullptr;
	pingImageWrite.dstSet = m_ComputeDescriptorSets[1];
	pingImageWrite.dstBinding = 0;
	pingImageWrite.dstArrayElement = 0;
	pingImageWrite.descriptorCount = 1;
	pingImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pingImageWrite.pImageInfo = ping;
	pingImageWrite.pBufferInfo = nullptr;
	pingImageWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet pongImageWrite{};
	pongImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	pongImageWrite.pNext = nullptr;
	pongImageWrite.dstSet = m_ComputeDescriptorSets[1];
	pongImageWrite.dstBinding = 1;
	pongImageWrite.dstArrayElement = 0;
	pongImageWrite.descriptorCount = 1;
	pongImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pongImageWrite.pImageInfo = pong;
	pongImageWrite.pBufferInfo = nullptr;
	pongImageWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	std::vector<VkWriteDescriptorSet> descriptorWrites =
	{
		pingImageWrite,
		pongImageWrite,
	};

	vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);
}

void RTXApp::CreateComputePipeline()
{
	VkPipelineLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.setLayoutCount = static_cast<uint32_t>(m_ComputeDescriptorSetsLayouts.size());
	createInfo.pSetLayouts = m_ComputeDescriptorSetsLayouts.data();

	VkResult error = vkCreatePipelineLayout(m_Device, &createInfo, nullptr, &m_ComputePipelineLayout);
	CHECK_VK_ERROR(error, "vkCreatePipelineLayout");

	m_ThresholdPass.CreatePipeline(s_ShadersFolder / "threshold.spv", m_ComputePipelineLayout);
	m_DownsamplePass.CreatePipeline(s_ShadersFolder / "downsample.spv", m_ComputePipelineLayout);
	m_FFTPass.CreatePipeline(s_ShadersFolder / "fft.spv", m_ComputePipelineLayout);
	m_UpsamplePass.CreatePipeline(s_ShadersFolder / "upsample.spv", m_ComputePipelineLayout);
	m_CompositePass.CreatePipeline(s_ShadersFolder / "composite.spv", m_ComputePipelineLayout);
}