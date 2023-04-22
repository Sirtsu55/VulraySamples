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
- Tested with CMake 3.22
- C++ 20 compiler

### Samples Structure
- `Base` Directory: Helper classes and functions

- `Shaders` Directory: All the shaders 

- `Assets` Directory: GLB 3D model files

- Points of Intrest are marked by ```[POI]``` in the Samples

### Samples Overview
| Sample		|  Description  |
|:----------	|:------------- |
| HelloTriangle | Simple triangle, with barycentric colors |
| DynamicTLAS   | Moving Triangles by updating TLAS every frame with different instance transforms |
| DynamicBLAS   | Transforming the scale of the BLAS every frame by updating it's vertex positions |
| Mesh Materials| This sample demonstrates how to organize geometries of a real scene into BLASses by loading a GLB scene and creating a BLAS for every mesh in the scene. Furthermore, uploads the material properties to the GPU and shades the geometries using their base color; no lighting yet.|
