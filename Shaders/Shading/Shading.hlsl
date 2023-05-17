#include "Shaders/Common/Material.hlsl"
#include "Shaders/Common/Functions.hlsl"
#include "Shaders/Common/Camera.hlsl"

#define PATH_SAMPLES 4
#define RECURSION_LENGTH 1


// vk::binding(binding, set)
[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;

[[vk::binding(1, 0)]] cbuffer uniformBuffer 
{ 
	float4x4 viewInverse;	
	float4x4 projInverse;
	float4 time; // time is in x, other values are unused / padding
};

[[vk::binding(2, 0)]] RWTexture2D<float4> image;
[[vk::binding(3, 0)]] RWStructuredBuffer<GPUMaterial> materials;
[[vk::binding(4, 0)]] StructuredBuffer<Vertex> VertexBuffer;
[[vk::binding(5, 0)]] StructuredBuffer<uint> IndexBuffer;

struct Payload
{
[[vk::location(0)]] float4 hitValue; // XYZ is color, W is attenuation
[[vk::location(1)]] float3 newRayDirection;
[[vk::location(2)]] float4 hitPoint; // XYZ, W is -1.0 if no hit or if it hit a light source and 1.0 if hit
[[vk::location(3)]] float3 lightContribution;
};

[shader("raygeneration")]
void rgen()
{
	uint3 LaunchID = DispatchRaysIndex();
	uint3 LaunchSize = DispatchRaysDimensions();

	const float2 pixelCenter = float2(LaunchID.xy) + float2(0.5, 0.5);
	const float2 uv = pixelCenter / float2(LaunchSize.xy);

	RayDesc ray = ConstructRay(viewInverse, projInverse, uv);


	Payload p;

	TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, p);

	float3 finalColor = pow(p.hitValue.rgb, float3(1.0/2.2, 1.0/2.2, 1.0/2.2)); // gamma correction

	image[int2(LaunchID.xy)] = float4(finalColor, 0.0);


}

float3 GetVertex(in uint vertBufferStart, in uint indexBufferStart, in uint index)
{
    uint idx = IndexBuffer[indexBufferStart + index];
	return VertexBuffer[vertBufferStart + idx].Position.xyz;
}

float3 GetNormal(in uint vertBufferStart, in uint indexBufferStart, in uint index)
{
	uint idx = IndexBuffer[indexBufferStart + index];
	return VertexBuffer[vertBufferStart + idx].Normal.xyz;
}

[shader("closesthit")]
void chit(inout Payload p, in float2 barycentrics)
{
	float3 v = WorldRayDirection();

	GPUMaterial mat = materials[InstanceID() + GeometryIndex()];

	float3 normals[3] = {
		GetNormal(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 0),
		GetNormal(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 1),
		GetNormal(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 2)
	};

	float3 normal = InterpolateTriangle(normals, barycentrics);

	normal = faceforward(normal, v, normal); // make sure normal is facing the camera

	uint seed = asint(time.x) * asint(v.x) * asint(v.y) * asint(v.z);

	float u1 = NextRandomFloat(seed);
	float u2 = NextRandomFloat(seed);
	
	float VdotN = dot(v, normal);
	
	float3 microfacetNormal = GGXRandomDirection(normal, v, mat.Roughness, u1, u2); // also called half vector
	float3 wo = -v; // outgoing direction of light, so it is the direction of the ray, because we are tracing from a camera
	float3 wi = reflect(v, microfacetNormal); // incoming direction of light


	p.hitValue.xyz = wi;
}


[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue.xyz = float3(0.1, 0.7, 1.0);
	p.hitPoint = float4(0.0, 0.0, 0.0, -1.0);
	p.lightContribution = float3(0.05, 0.05, 0.05);
	p.newRayDirection = float3(0.0, 0.0, 0.0);
}

