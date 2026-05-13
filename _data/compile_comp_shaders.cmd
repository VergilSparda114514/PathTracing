@echo off

setlocal
set SOURCE_FOLDER="./../Source/shaders/"
set BINARIES_FOLDER="./shaders/"

D:/VulkanSDK/1.4.321.1/Bin/glslc.exe %SOURCE_FOLDER%threshold.comp -o %BINARIES_FOLDER%threshold.spv
D:/VulkanSDK/1.4.321.1/Bin/glslc.exe %SOURCE_FOLDER%downsample.comp -o %BINARIES_FOLDER%downsample.spv
D:/VulkanSDK/1.4.321.1/Bin/glslc.exe %SOURCE_FOLDER%upsample.comp -o %BINARIES_FOLDER%upsample.spv
D:/VulkanSDK/1.4.321.1/Bin/glslc.exe %SOURCE_FOLDER%fft.comp -o %BINARIES_FOLDER%fft.spv
D:/VulkanSDK/1.4.321.1/Bin/glslc.exe %SOURCE_FOLDER%composite.comp -o %BINARIES_FOLDER%composite.spv

pause