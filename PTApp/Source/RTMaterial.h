#pragma once

#include "VulkanHelpers.h"

struct RTMaterial
{
    std::string                 name{};
    size_t                      diffuseMap = 0;
    size_t                      bumpMap = 0;
};