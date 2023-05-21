
// This needs to match the index in c++ code
enum MaterialType : uint
{
    Opaque = 0, // opaque material
    Emissive = 1 // emissive material
};

struct Vertex // has to match the layout alignment in c++ code
{
    float4 Position;
    float4 Normal;
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

float3 GetVertex(in StructuredBuffer<Vertex> vertBuffer,
    in StructuredBuffer<uint> idxBuffer, 
    in uint vertBufferStart,
    in uint indexBufferStart,
    in uint index)
{
    uint idx = idxBuffer[indexBufferStart + index];
	return vertBuffer[vertBufferStart + idx].Position.xyz;
}

float3 GetNormal(in StructuredBuffer<Vertex> vertBuffer,
    in StructuredBuffer<uint> idxBuffer, 
    in uint vertBufferStart,
    in uint indexBufferStart,
    in uint index)
{
    uint idx = idxBuffer[indexBufferStart + index];
	return vertBuffer[vertBufferStart + idx].Normal.xyz;
}

float3 InterpolateTriangle(float3 vertexAttribute[3], in float2 barycentrics)
{
    return vertexAttribute[2] +
        barycentrics.x * (vertexAttribute[1] - vertexAttribute[2]) +
        barycentrics.y * (vertexAttribute[0] - vertexAttribute[2]); // barycentric interpolation
}
