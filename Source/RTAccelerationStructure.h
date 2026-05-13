#pragma once

#include "framework/VulkanHelpers.h"

struct RTAccelerationStructure
{
    VulkanHelpers::Buffer                   buffer;
    VkAccelerationStructureKHR              accelerationStructure;
    VkDeviceAddress                         handle;
};