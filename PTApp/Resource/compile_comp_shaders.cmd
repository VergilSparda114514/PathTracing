@echo off

setlocal
set SOURCE_FOLDER="./../Source/shaders/"
set BINARIES_FOLDER="./shaders/"

D:/VulkanSDK/1.4.321.1/Bin/glslc.exe %SOURCE_FOLDER%composite.comp -o %BINARIES_FOLDER%composite.spv

pause