# Blaze

A WIP rendering engine in Vulkan 1.1 using C++17
Made by Anish Bhobe

[![Damaged Helmet glTF](presentation/basic-helm.png)](https://youtu.be/gmUxLgRCC1o)

### Features (Current and Planned)
- [x] Forward Rendering
- [ ] glTF 2.0 Support
  - [x] Load Vertex Data
  - [x] Load Material Data
  - [ ] Load Animation Data
  - [ ] Load Camera
  - [ ] Load Lights
  - [ ] Support Specular Materials
- [x] PBR
- [x] IBL
- [x] Shadows v1
  - [x] Omnidirectional Cubemap Shadows
  - [x] Directional Shadows
  - [x] Cascaded Shadows
  - [x] PCF
- [ ] Smooth Descriptor Creation Pipeline
- [ ] Deferred Rendering
- [ ] Post Process Stack
- [ ] Forward+ Rendering
- [ ] Global Illumination
- [ ] Shadows v2
  - [ ] Spot Lights
  - [ ] Area Lights
  - [ ] Omnidirectional Dual Paraboloid Shadows
  - [ ] Perspective Shadow Mapping
- [ ] Animation
- [ ] Particle Effect

### Dependencies:
- [GLFW 3](https://github.com/glfw/glfw)
- [GLM](https://github.com/g-truc/glm)
- [TinyGltf](https://github.com/syoyo/tinygltf)
- [stb_image](https://github.com/nothings/stb)
- [Dear ImGUI](https://github.com/ocornut/imgui)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [SPRIV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)

### Most Recent Work: Cascaded Shadow Maps implemented.

![Cascades in Sponza](presentation/csm-vis.png)
