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

struct HitInfo
{
	float3 Color;
	float Atennuation;
	float3 LightContribution;
	bool TerminateRay;
};
struct NewRayInfo
{
	float3 Direction;
	float3 Origin;
};

struct Payload
{
[[vk::location(0)]] HitInfo Info;
[[vk::location(1)]] NewRayInfo RayInfo;
};

[shader("raygeneration")]
void rgen()
{
	uint3 LaunchID = DispatchRaysIndex();
	uint3 LaunchSize = DispatchRaysDimensions();

	const float2 pixelCenter = float2(LaunchID.xy) + float2(0.5, 0.5);
	const float2 uv = pixelCenter / float2(LaunchSize.xy);

	// Construct a ray from the camera for the first ray
	// For later recursive rays, the ray direction will be set in the closest hit shader
	RayDesc ray = ConstructRay(viewInverse, projInverse, uv);

	Payload p;

	uint RecursionDepth = 0;
	float4 ColorAttenuation = float4(0.0, 0.0, 0.0, 1.0);
	while(RecursionDepth < RECURSION_LENGTH)
	{
		RecursionDepth++;

		TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, p);
		ray.Origin = p.RayInfo.Origin;
		ray.Direction = p.RayInfo.Direction;

		ColorAttenuation.xyz += p.Info.Color * ColorAttenuation.w;
		ColorAttenuation.w *= p.Info.Atennuation;
		if(p.Info.TerminateRay)
			break;
	}
	
	float3 finalColor = (ColorAttenuation.xyz) / float(RecursionDepth);
	finalColor = pow(finalColor, float3(1.0/2.2, 1.0/2.2, 1.0/2.2)); // gamma correction

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
	// normal = faceforward(normal, v, normal); // make sure normal is facing the camera


	uint seed = asint(time.x) * asint(v.x) * asint(v.y) * asint(v.z);

	float u1 = NextRandomFloat(seed);
	float u2 = NextRandomFloat(seed);
	
	float3 microfacetNormal = GGXRandomDirection(normal, mat.Roughness, u1, u2); // also called half vector

	float3 wo = -v; // outgoing direction of light, so it is the direction of the ray, because we are tracing from a camera

	float3 wi = reflect(v, microfacetNormal); // incoming direction of light
	p.RayInfo.Direction = wi;
	p.RayInfo.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

	p.Info.LightContribution = any(mat.Emissive) ? mat.Emissive : float3(0.0, 0.0, 0.0); // if emissive is set, then this is a light source
	p.Info.TerminateRay = any(mat.Emissive); 
	p.Info.Color = microfacetNormal;
	p.Info.Atennuation = 1.0;

}


[shader("miss")]
void miss(inout Payload p)
{
	p.Info.Color = float3(0.0, 0.0, 0.0);
	p.Info.Atennuation = 1.0;
	p.Info.LightContribution = float3(0.2, 0.2, 0.0);
	p.Info.TerminateRay = true;
}

