#include "Shaders/Common/Material.hlsl"
#include "Shaders/Common/Random.hlsl"
#include "Shaders/Common/Sampling.hlsl"
#include "Shaders/Common/Ray.hlsl"

#define PATH_SAMPLES 4
#define RECURSION_LENGTH 4

// vk::binding(binding, set)
[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;
[[vk::binding(1, 0)]] cbuffer uniformBuffer 
{ 
	float4x4 viewInverse;	
	float4x4 projInverse;
	float4 otherInfo; // time is in x, other values are unused / padding
};
[[vk::binding(2, 0)]] RWTexture2D<float4> image;
[[vk::binding(3, 0)]] StructuredBuffer<GPUMaterial> materials;
[[vk::binding(4, 0)]] StructuredBuffer<Vertex> VertexBuffer;
[[vk::binding(5, 0)]] StructuredBuffer<uint> IndexBuffer;
[[vk::binding(6, 0)]] RWTexture2D<float4> accumulationImage;

struct HitInfo
{
	float3 Color;
	float Attenuation;
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


	// Construct a ray from the camera for the first ray
	// For later recursive rays, the ray direction will be set in the closest hit shader

	Payload p;
	p.Info.Attenuation = 1.0;


	uint TotalRaysShot = 0;
	uint PathSamples = 0;
	float3 Color = float3(0, 0, 0);

	float time = otherInfo.x;
	uint frameCount = asuint(otherInfo.y);

	uint2 randSeed = uint2(LaunchID.xy * time);

	while(PathSamples < PATH_SAMPLES)
	{
		float2 rand = float2(NextRandomFloat(randSeed.x), NextRandomFloat(randSeed.y));
		const float2 pixelCenter = float2(LaunchID.xy) + rand;
		const float2 uv = pixelCenter / float2(LaunchSize.xy);
		RayDesc ray = ConstructRay(viewInverse, projInverse, uv);

		uint RecursionDepth = 0;
		while(RecursionDepth < RECURSION_LENGTH)
		{
			RecursionDepth++;

			TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, p);
			ray.Origin = p.RayInfo.Origin;
			ray.Direction = p.RayInfo.Direction;

			Color += p.Info.Color;

			if(p.Info.TerminateRay)
				break;
		}
		TotalRaysShot += RecursionDepth;
		PathSamples++;
	}
	
	float3 Radiance = (Color * p.Info.LightContribution) / float(TotalRaysShot);

	//Apply accumulation
	float3 Accumulated = accumulationImage[int2(LaunchID.xy)].xyz;

	float3 PixelColor = (Radiance + Accumulated) / float(frameCount + 1);

	PixelColor = pow(PixelColor, float3(1.0/2.2, 1.0/2.2, 1.0/2.2)); // gamma correction

	image[int2(LaunchID.xy)] = float4(PixelColor, 0.0);

	if(frameCount == 0)
		accumulationImage[int2(LaunchID.xy)] = float4(Radiance, 0.0);
	else
		accumulationImage[int2(LaunchID.xy)] = float4(Radiance + Accumulated, 0.0);

}


[shader("closesthit")]
void chit(inout Payload p, in float2 barycentrics)
{

	GPUMaterial mat = materials[InstanceID() + GeometryIndex()];

	float3 normals[3] = {
		GetNormal(VertexBuffer, IndexBuffer, mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 0),
		GetNormal(VertexBuffer, IndexBuffer, mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 1),
		GetNormal(VertexBuffer, IndexBuffer, mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 2)
	};

	float3 v = WorldRayDirection();

	float3 normal = InterpolateTriangle(normals, barycentrics);
	normal = faceforward(normal, v, normal); // make sure normal is facing the camera

	float time = otherInfo.x;
	uint seed = asint(time) * asint(v.x) * asint(v.y) * asint(v.z);
	float u1 = NextRandomFloat(seed);
	float u2 = NextRandomFloat(seed);

	float3 newDir = SampleCosineHemisphere(normal, u1, u2);

	float cosTheta = dot(newDir, normal);

	float pdf = CosineHemispherePDF(cosTheta);
	
	float RayContribution = p.Info.Attenuation * pdf;

	float3 color = (mat.BaseColor * RayContribution);


	// Payload Update

	// if the material is emissive, terminate the ray and return the emissive color
	bool isLight = any(mat.Emissive);
	p.Info.LightContribution = isLight ? mat.Emissive * 100: float3(0.0, 0.0, 0.0);
	// terminate the ray if the material is emissive
	p.Info.TerminateRay = isLight;
	// set the new color and attenuation for the next ray
	p.Info.Color = color;
	p.Info.Attenuation *= (1.0 - RayContribution);
	// set the new ray direction and origin
	p.RayInfo.Direction = newDir;
	p.RayInfo.Origin = GetIntersectionPosition();
}


[shader("miss")]
void miss(inout Payload p)
{
	p.Info.Color = float3(0.0, 0.0, 0.0);
	p.Info.Attenuation = 1.0;
	p.Info.LightContribution = float3(0.0, 0.0, 0.0);
	p.Info.TerminateRay = true;
}

