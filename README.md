# Blaze
#### Version 1.1

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
- [x] Smooth Descriptor Creation Pipeline
- [x] Deferred Rendering
- [x] Post Process Stack
- [ ] Ambient Occlusion
  - [x] SSAO
  - [ ] HBAO
  - [ ] VXAO/SDFAO
- [ ] Reflection
  - [ ] ScreenSpace Reflection
  - [ ] Cubemap Reflection
- [ ] Forward+ Rendering
- [ ] Global Illumination
  - [ ] Precomputed Radiance Transfer
  - [ ] Voxel Cone Tracing
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
- [SPRIV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)
- [Renderdoc API](https://github.com/baldurk/renderdoc)

### Most Recent Work: Cascaded Shadow Maps implemented.

![Cascades in Sponza](presentation/csm-vis.png)

### Deferred Shading Implemented.

Blaze now has a deferred renderer with spherical volumes.  
The unoptimized GBuffer is:  
Position - R16G16B16A16 SFLOAT (3 channel position + alpha linear distance)  
Albedo - R8G8B8A8 UNORM (3 channel color + alpha unused)  
Normal - R16G16B16A16 SFLOAT (3 channel world space normals + alpha unused)  
Physical Desc - R8G8B8A8 UNORM (R: Occlusion, G: Metallic, B: Roughness, A: Unused)  
Emission - R8G8B8A8 SFLOAT (3 channel emission + alpha unused)  

### Added SSAO

SSAO has been added with 64 samples in a hemisphere, and 5x5 blur at half resolution.
Next up to reduce the sample requirement by using low discrepancy samples.

Performance hit ~6ms at 1920x1080 full resolution SSAO.

![Full resolution SSAO in Sponza](presentation/ssao.png)

### Next up: Forward+/Clustered Forward renderer
