#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <tiny_gltf.h>
#include "Camera.h"
#include <glm/gtx/matrix_major_storage.hpp>

struct Mesh
{
    std::vector<uint32_t> GeometryReferences;

    // std::vector<uint32_t> MaterialReferences;

    // transform is applied to all geometries in the mesh
    // stored in row major order, similar to VkTransformMatrixKHR
    glm::mat3x4 Transform = glm::mat3x4(1.0f); 
};

struct Geometry
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
    std::vector<Geometry> Geometries;

    std::vector<Mesh> Meshes;

};


class MeshLoader
{
public:
    Scene LoadGLBMesh(const std::string& path);

private:


    void AddMeshToScene(const tinygltf::Mesh& mesh, const tinygltf::Model& model, Scene& outScene);

    tinygltf::TinyGLTF mLoader;
};

