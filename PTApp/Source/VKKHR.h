#pragma once

#include <vulkan/vulkan.h>

#include <cassert>

#define LoadPFN(dev, var, name) do{\
	var = (PFN_##name)vkGetDeviceProcAddr(dev, #name);\
	if (!var) assert(var && "Failed to load Vulkan function");\
} while(false)

static PFN_vkDestroyAccelerationStructureKHR DestroyAccelerationStructureKHR = nullptr;
static PFN_vkCmdTraceRaysKHR CmdTraceRaysKHR = nullptr;
static PFN_vkGetAccelerationStructureBuildSizesKHR GetAccelerationStructureBuildSizesKHR = nullptr;
static PFN_vkGetRayTracingShaderGroupHandlesKHR GetRayTracingShaderGroupHandlesKHR = nullptr;
static PFN_vkGetAccelerationStructureDeviceAddressKHR GetAccelerationStructureDeviceAddressKHR = nullptr;
static PFN_vkCreateAccelerationStructureKHR CreateAccelerationStructureKHR = nullptr;
static PFN_vkCreateRayTracingPipelinesKHR CreateRayTracingPipelinesKHR = nullptr;
static PFN_vkCmdBuildAccelerationStructuresKHR CmdBuildAccelerationStructuresKHR = nullptr;

namespace VKKHR
{
	static VkDevice s_Device = nullptr;

	static void LoadPFNs(VkDevice device)
	{
		s_Device = device;

		LoadPFN(device, DestroyAccelerationStructureKHR, vkDestroyAccelerationStructureKHR);
		LoadPFN(device, CmdTraceRaysKHR, vkCmdTraceRaysKHR);
		LoadPFN(device, GetAccelerationStructureBuildSizesKHR, vkGetAccelerationStructureBuildSizesKHR);
		LoadPFN(device, GetRayTracingShaderGroupHandlesKHR, vkGetRayTracingShaderGroupHandlesKHR);
		LoadPFN(device, GetAccelerationStructureDeviceAddressKHR, vkGetAccelerationStructureDeviceAddressKHR);
		LoadPFN(device, CreateAccelerationStructureKHR, vkCreateAccelerationStructureKHR);
		LoadPFN(device, CreateRayTracingPipelinesKHR, vkCreateRayTracingPipelinesKHR);
		LoadPFN(device, CmdBuildAccelerationStructuresKHR, vkCmdBuildAccelerationStructuresKHR);
	}
}