# VulraySamples
## Collection of samples for Vulray

This repository contains a collection of samples on how to use the Vulray raytracing library 
https://github.com/Sirtsu55/Vulray. The Samples are heavily commented to help understand what is going on.
The samples inherit from a base Application class, that creates a window, presents an image to the screen and creates 
a Vulkan device, and Instance using Vulray's Vulkan builder. It also has protected members that 
the sample apllications can use in their code. Check [Apllication.h](https://github.com/Sirtsu55/VulraySamples/blob/master/Base/Application.h)
to see what is declared there and [Apllication.cpp](https://github.com/Sirtsu55/VulraySamples/blob/master/Base/Application.cpp) for implementation.

### Building
- Meet the requirements of [Vulray](https://github.com/Sirtsu55/Vulray)
- Tested with CMake 3.25
- C++ 20 compiler

### Samples Structure
- `Base` Directory: Helper classes and functions

- `Shaders` Directory: All the shaders 

- `Assets` Directory: GLB 3D model files

- Points of Intrest are marked by ```[POI]``` in the Samples

- Move Freely in the scene using `WASD` and rotate camera by `left-click + mouse` and roll camera by `Q-E`
### Samples Overview
| Sample		|  Description  |
|:----------	|:------------- |
| HelloTriangle <img src=https://user-images.githubusercontent.com/65868911/233778107-bcb63256-bec0-4502-895e-b8c23f61846d.png>| Simple triangle, with barycentric colors |
| DynamicTLAS  <img src=https://user-images.githubusercontent.com/65868911/233778012-5fe85298-39ac-4e98-95b8-7489657e76a2.png>| Moving triangles by updating TLAS every frame with different instance transforms |
| DynamicBLAS | Transforming the scale of a triangle BLAS every frame by updating it's vertex positions |
| BoxIntersections <img src=https://github.com/Sirtsu55/VulraySamples/assets/65868911/e1dba8a3-bf47-4315-ab60-72da16475c91> | Custom AABB box intersection with custom intersection shader and AABB BLAS primitives|
| Compaction | Using compaction to compact the BLAS, which significantly reduces the memory footprint. Almost half of the original required size |
| Callable <img src=https://github.com/Sirtsu55/VulraySamples/assets/65868911/64369c75-eb27-4ba1-ab10-8ce80f4e99c0>| Using callable shaders to shade our triangles uniquely |
| Mesh Materials <img src=https://user-images.githubusercontent.com/65868911/233778450-970dc17d-fa0e-42cc-8e20-f50312fdeb9d.png>| This sample demonstrates how to organize geometries of a real scene into BLASses by loading a GLB scene and creating a BLAS for every mesh in the scene. Furthermore, uploads the material properties to the GPU and shades the geometries using their base color; no lighting yet.|
| Shading	<img src=https://github.com/Sirtsu55/VulraySamples/assets/65868911/277e04f5-9a10-4c4e-8f42-043c7f4f74ba>| This sample shows how to implement Lambertian diffuse shading and implements color accumulation to reduce noise over still frames. This sample is mainly about shader code. So look at the shaders used in this sample. |

