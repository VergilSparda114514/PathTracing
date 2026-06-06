#include "ShaderBindingTable.h"

#include "VKKHR.h"

static uint32_t AlignUp(const uint32_t value, const uint32_t align)
{
	return (value + align - 1) & ~(align - 1);
}

void ShaderBindingTable::Initialize(const uint32_t numHitGroups, const uint32_t numMissGroups, const uint32_t shaderHandleSize, const uint32_t shaderGroupAlignment)
{
	m_ShaderHandleSize = shaderHandleSize;
	m_ShaderGroupAlignment = shaderGroupAlignment;
	m_NumHitGroups = numHitGroups;
	m_NumMissGroups = numMissGroups;

	m_NumHitShaders.resize(numHitGroups, 0u);
	m_NumMissShaders.resize(numMissGroups, 0u);

	m_Stages.clear();
	m_Groups.clear();
}

void ShaderBindingTable::Destroy()
{
	m_NumHitShaders.clear();
	m_NumMissShaders.clear();
	m_Stages.clear();
	m_Groups.clear();

	m_SBTBuffer.Destroy();
}

void ShaderBindingTable::SetRaygenStage(const VkPipelineShaderStageCreateInfo& stage)
{
	// this shader stage should go first!
	assert(m_Stages.empty());
	m_Stages.push_back(stage);

	VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
	groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groupInfo.generalShader = 0;
	groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	m_Groups.push_back(groupInfo); // group 0 is always for raygen
}

void ShaderBindingTable::AddStageToHitGroup(const std::vector<VkPipelineShaderStageCreateInfo>& stages, const uint32_t groupIndex)
{
	// raygen stage should go first!
	assert(!m_Stages.empty());

	assert(groupIndex < m_NumHitShaders.size());
	assert(!stages.empty() && stages.size() <= 3);// only 3 hit shaders per group (intersection, any-hit and closest-hit)
	assert(m_NumHitShaders[groupIndex] == 0);

	uint32_t offset = 1; // there's always raygen shader

	for (uint32_t i = 0; i <= groupIndex; i++)
	{
		offset += m_NumHitShaders[i];
	}

	auto itStage = m_Stages.begin() + offset;
	m_Stages.insert(itStage, stages.begin(), stages.end());

	VkRayTracingShaderGroupCreateInfoKHR groupInfo{};
	groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
	groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

	for (size_t i = 0; i < stages.size(); i++)
	{
		const VkPipelineShaderStageCreateInfo& stageInfo = stages[i];
		const uint32_t shaderIdx = static_cast<uint32_t>(offset + i);

		if (stageInfo.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		{
			groupInfo.closestHitShader = shaderIdx;
		}

		else if (stageInfo.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
		{
			groupInfo.anyHitShader = shaderIdx;
		}
	};

	m_Groups.insert((m_Groups.begin() + 1 + groupIndex), groupInfo);

	m_NumHitShaders[groupIndex] += static_cast<uint32_t>(stages.size());
}

void ShaderBindingTable::AddStageToMissGroup(const VkPipelineShaderStageCreateInfo& stage, const uint32_t groupIndex)
{
	// raygen stage should go first!
	assert(!m_Stages.empty());

	assert(groupIndex < m_NumMissShaders.size());
	assert(m_NumMissShaders[groupIndex] == 0); // only 1 miss shader per group

	uint32_t offset = 1; // there's always raygen shader

	// now skip all hit shaders
	for (const uint32_t numHitShader : m_NumHitShaders)
	{
		offset += numHitShader;
	}

	for (uint32_t i = 0; i <= groupIndex; i++)
	{
		offset += m_NumMissShaders[i];
	}

	m_Stages.insert(m_Stages.begin() + offset, stage);

	// group create info 
	VkRayTracingShaderGroupCreateInfoKHR groupInfo{};
	groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groupInfo.generalShader = offset;
	groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

	// group 0 is always for raygen, then go hit shaders
	m_Groups.insert((m_Groups.begin() + (groupIndex + 1 + m_NumHitGroups)), groupInfo);

	m_NumMissShaders[groupIndex]++;
}

uint32_t ShaderBindingTable::GetGroupsStride() const
{
	return m_ShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetNumGroups() const
{
	return 1 + m_NumHitGroups + m_NumMissGroups;
}

uint32_t ShaderBindingTable::GetRaygenOffset() const
{
	return 0;
}

uint32_t ShaderBindingTable::GetRaygenSize() const
{
	return m_ShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetHitGroupsOffset() const
{
	return GetRaygenOffset() + GetRaygenSize();
}

uint32_t ShaderBindingTable::GetHitGroupsSize() const
{
	return m_NumHitGroups * m_ShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetMissGroupsOffset() const
{
	return GetHitGroupsOffset() + GetHitGroupsSize();
}

uint32_t ShaderBindingTable::GetMissGroupsSize() const
{
	return m_NumMissGroups * m_ShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetNumStages() const
{
	return static_cast<uint32_t>(m_Stages.size());
}

const VkPipelineShaderStageCreateInfo* ShaderBindingTable::GetStages() const
{
	return m_Stages.data();
}

const VkRayTracingShaderGroupCreateInfoKHR* ShaderBindingTable::GetGroups() const
{
	return m_Groups.data();
}

uint32_t ShaderBindingTable::GetSBTSize() const
{
	return GetNumGroups() * m_ShaderGroupAlignment;
}

bool ShaderBindingTable::CreateSBT(VkDevice device, VkPipeline rtPipeline)
{
	VKKHR::LoadPFNs(device);

	const size_t sbtSize = GetSBTSize();

	VkResult error = m_SBTBuffer.Create(sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "mSBT.Create");

	if (VK_SUCCESS != error)
	{
		return false;
	}

	// get shader group handles
	std::vector<uint8_t> groupHandles(GetNumGroups() * m_ShaderHandleSize);
	error = GetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, GetNumGroups(), groupHandles.size(), groupHandles.data());
	CHECK_VK_ERROR(error, L"vkGetRayTracingShaderGroupHandlesKHR");

	// now we fill our SBT
	uint8_t* mem = static_cast<uint8_t*>(m_SBTBuffer.Map());
	for (size_t i = 0; i < GetNumGroups(); i++)
	{
		memcpy(mem, groupHandles.data() + i * m_ShaderHandleSize, m_ShaderHandleSize);
		mem += m_ShaderGroupAlignment;
	}
	m_SBTBuffer.Unmap();

	return (VK_SUCCESS == error);
}

VkDeviceAddress ShaderBindingTable::GetSBTAddress() const
{
	return VulkanHelpers::GetBufferDeviceAddress(m_SBTBuffer).deviceAddress;
}