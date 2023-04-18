

struct GPUMaterial // has to be aligned to 16 bytes
{
    float4 BaseColor;
    float Metallic;
    float Roughness;
    float2 Padding; // add padding to make sure the struct is 16 byte aligned
};