#pragma once

#include "framework/VulkanHelpers.h"

struct RTMaterial
{
    std::string                 name;
    VulkanHelpers::Image        texture;
    VulkanHelpers::Image        normalMap;
};