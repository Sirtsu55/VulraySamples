#include <glm/glm.hpp>

enum class MaterialType : uint32_t
{
    Opaque = 0, // opaque material
    Emissive = 1 // emissive material
};

struct GPUMaterial // has to be aligned to 16 bytes
{
    glm::vec3 BaseColor = glm::vec3(1.0f);
    float Metallic = 1.0f;
    
    glm::vec3 Emissive = glm::vec3(0.0f);
    float Roughness = 1.0f;
    MaterialType Type; // add padding to make sure the struct is 16 byte aligned

    float Padding[3];
}; 