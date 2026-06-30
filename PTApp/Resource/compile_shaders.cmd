@echo off

setlocal
set GLSL_COMPILER=%VULKAN_SDK%/Bin/glslangValidator.exe
set SOURCE_FOLDER="./../Source/Shaders/"
set BINARIES_FOLDER="./Shaders/"

:: raygen shaders
%GLSL_COMPILER% --target-env vulkan1.2 -V -S rgen %SOURCE_FOLDER%ray_gen.glsl -o %BINARIES_FOLDER%ray_gen.spv

:: closest-hit shaders
%GLSL_COMPILER% --target-env vulkan1.2 -V -S rchit %SOURCE_FOLDER%ray_chit.glsl -o %BINARIES_FOLDER%ray_chit.spv

:: miss shaders
%GLSL_COMPILER% --target-env vulkan1.2 -V -S rmiss %SOURCE_FOLDER%ray_miss.glsl -o %BINARIES_FOLDER%ray_miss.spv

pause