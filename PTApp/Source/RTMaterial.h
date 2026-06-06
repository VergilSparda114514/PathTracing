#pragma once

#include "VulkanHelpers.h"

struct RTMaterial
{
    std::string                 name;
    VulkanHelpers::Image        texture;
    VulkanHelpers::Image        bumpMap;
};