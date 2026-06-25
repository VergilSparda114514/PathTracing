@echo off

%VULKAN_SDK%/Bin/glslc.exe ../Source/shaders/composite.comp -o ./shaders/composite.spv

pause