
// This needs to match the index in c++ code
enum MaterialType : uint
{
    Opaque = 0, // opaque material
    Emissive = 1 // emissive material
};

struct Vertex // has to match the layout alignment in c++ code
{
    float4 Position;
};

struct GPUMaterial // has to be aligned to 16 bytes
{
    float3 BaseColor;
    float Metallic;
    float3 Emissive;
    float Roughness;
    uint VertBufferStart;
    uint IndexBufferStart;

    float2 Padding;
};