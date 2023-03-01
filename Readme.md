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
- ```Application::Start()``` - Called once when launching
- ```Application::Update(...)``` - Called every frame
- ```Application::Stop()``` - Called when quitting window
- Samples have helper functions to spit the code

### Go and look at the samples...