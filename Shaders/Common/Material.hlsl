
// This needs to match the index in c++ code
enum MaterialType : unsigned int
{
    Opaque = 0, // opaque material
    Emissive = 1 // emissive material
};

struct GPUMaterial // has to be aligned to 16 bytes
{
    float3 BaseColor;
    float Metallic;
    float3 Emissive;
    float Roughness;
    unsigned int Type;
    float3 Padding;
};