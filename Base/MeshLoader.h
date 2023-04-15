#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <tiny_gltf.h>
#include "Camera.h"

struct Mesh
{
    
    std::vector<uint8_t> Vertices;
    std::vector<uint8_t> Indices;
    uint32_t VertexSize = 0;
    vk::Format VertexFormat = vk::Format::eR32G32B32Sfloat;
    uint32_t IndexSize = 0;
    vk::IndexType IndexFormat = vk::IndexType::eUint32;
    
    glm::mat4 Transform = glm::mat4(1.0f);
};

struct Scene
{
    std::vector<Camera> Cameras;
    std::vector<Mesh> Meshes;
};


class MeshLoader
{
public:
    Scene LoadGLBMesh(const std::string& path);

private:

    tinygltf::TinyGLTF mLoader;
};

