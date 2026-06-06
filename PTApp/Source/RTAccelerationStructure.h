#pragma once

#include "VulkanHelpers.h"

struct RTAccelerationStructure
{
    VulkanHelpers::Buffer                   buffer;
    VkAccelerationStructureKHR              accelerationStructure;
    VkDeviceAddress                         handle;
};