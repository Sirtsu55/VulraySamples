#pragma once

#include <glm/glm.hpp>

enum class MaterialType : uint32_t
{
    Opaque = 0, // opaque material
    Emissive = 1 // emissive material
};

struct Vertex
{
    glm::vec3 Position;
    float padding; // this is to assure layout requirements when accessing from SSBO in shaders
    glm::vec3 Normal;
    float padding2;
};

struct GPUMaterial // has to be aligned to 16 bytes
{
    glm::vec3 BaseColor = glm::vec3(1.0f);
    float Metallic = 1.0f;
    
    glm::vec3 Emissive = glm::vec3(0.0f);
    float Roughness = 1.0f;

    uint32_t VertBufferOffset = 0;
    uint32_t IndexBufferOffset = 0;

    float Padding[2]; 
}; 