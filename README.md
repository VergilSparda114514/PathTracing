# Path Tracing

A simple path tracing project implemented in Vulkan \& C++

## Requirements

* [Visual Studio 2022](https://visualstudio.com) (not strictly required, however included setup scripts only support this)
* [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) (preferably a recent version)

## Getting Started

1. Clone using `git clone --recursive https://github.com/VergilSparda114514/PathTracing.git`
2. Run `compile_shaders.cmd` or `compile_shaders.sh` and `compile_comp_shaders.cmd` in the `PTApp/Resource` folder
3. Run `Setup.bat` in the `scripts` folder
4. Open the `.sln` file and hit F5 to compile and run the project

### 3rd party libaries

* [Dear ImGui](https://github.com/ocornut/imgui)
* [GLFW](https://github.com/glfw/glfw)
* [stb\_image](https://github.com/nothings/stb)
* [GLM](https://github.com/g-truc/glm)
* [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)

### TODO (by priority)
* ReSTIR GI
* FFT Bloom
* Docking

