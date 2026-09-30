# Path Tracing

A simple path tracing project implemented in Vulkan \& C++
![White Box](Screenshots/white_box.png)
Credits to [iOrange](https://iorange.github.io) for the tutorial

## Requirements

* [Visual Studio 2022](https://visualstudio.com) (Recommended for Windows)
* [Vulkan SDK](https://vulkan.lunarg.com) (Preferably a recent version)

# Getting Started

### Controls
- Hold down right mouse button to unlock the camera
- WASD to move the camera
- QE to elevate the camera
- F to toggle UI

### [More Scenes](https://drive.google.com/drive/folders/1M_iqT96lMv0s2-hHGg3iR9SLaHJWcwY2?usp=drive_link)

## Windows

1. Clone using `git clone --recursive https://github.com/VergilSparda114514/PathTracing.git`
2. Run the `Setup.bat` script located in the `scripts` folder
3. Open the `Path Tracing.slnx` solution file in Visual Studio and hit `F5` to build and run the project

## Linux

1. Clone using `git clone --recursive https://github.com/VergilSparda114514/PathTracing.git`
2. Run the `Setup.sh` script located in the `scripts` folder
3. Run `make` in the root folder of the project to build the project

## Screenshots
![Sponza 1](Screenshots/sponza1.png)
![Sponza 2](Screenshots/sponza2.png)

### 3rd party libaries

* [Dear ImGui](https://github.com/ocornut/imgui)
* [GLFW](https://github.com/glfw/glfw)
* [stb\_image](https://github.com/nothings/stb)
* [GLM](https://github.com/g-truc/glm)
* [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)

### TODO
* [x] BSDF
* [ ] FFT Bloom
* [ ] ReSTIR GRIS
* [ ] NVIDIA DLSS
* [ ] NVIDIA Ray Reconstruction
* [ ] AMD FSR
* [ ] AMD Ray Regeneration
* [ ] Gaussian Splatting

