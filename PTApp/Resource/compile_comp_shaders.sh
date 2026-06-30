#!/bin/sh

${VULKAN_SDK}/Bin/glslc.exe "../Source/shaders/composite.comp" -o "./shaders/composite.spv"
${VULKAN_SDK}/Bin/glslc.exe "../Source/shaders/bloom/fft.comp" -o "./shaders/fft.spv"
${VULKAN_SDK}/Bin/glslc.exe "../Source/shaders/bloom/pad.comp" -o "./shaders/pad.spv"