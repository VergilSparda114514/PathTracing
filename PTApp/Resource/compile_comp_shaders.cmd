@echo off

setlocal
set GLSL_COMPILER=%VULKAN_SDK%/Bin/glslc.exe
set SOURCE_FOLDER="./../Source/Shaders/"
set BINARIES_FOLDER="./Shaders/"

%GLSL_COMPILER% %SOURCE_FOLDER%composite.comp -o %BINARIES_FOLDER%composite.spv
%GLSL_COMPILER% %SOURCE_FOLDER%bloom/fft.comp -o %BINARIES_FOLDER%fft.spv
%GLSL_COMPILER% %SOURCE_FOLDER%bloom/fft_kernel.comp -o %BINARIES_FOLDER%fft_kernel.spv
%GLSL_COMPILER% %SOURCE_FOLDER%bloom/pad.comp -o %BINARIES_FOLDER%pad.spv

pause