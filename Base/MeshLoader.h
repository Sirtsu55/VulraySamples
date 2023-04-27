#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <tiny_gltf.h>
#include "Camera.h"
#include <glm/gtx/matrix_major_storage.hpp>
#include "GPUMaterial.h"

struct Mesh
{
    std::vector<uint32_t> GeometryReferences;

    // transform is applied to all geometries in the mesh
    // stored in row major order, similar to VkTransformMatrixKHR
    glm::mat3x4 Transform = glm::mat3x4(1.0f); 
};

struct GeometryMaterial
{
    glm::vec4 BaseColorFactor = glm::vec4(1.0f);
    float MetallicFactor = 1.0f;
    float RoughnessFactor = 1.0f;
    glm::vec3 EmissiveFactor = glm::vec3(0.0f);

    std::vector<uint8_t> BaseColorTexture;
    std::vector<uint8_t> MetallicRoughnessTexture;
    std::vector<uint8_t> NormalTexture;
};

struct Geometry
{
    
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;
    
    glm::mat4 Transform = glm::mat4(1.0f);

    GeometryMaterial Material;
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


    void AddMeshToScene(const tinygltf::Mesh& mesh, tinygltf::Model& model, Scene& outScene);

    tinygltf::TinyGLTF mLoader;
};

