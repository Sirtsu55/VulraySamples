#include "Shaders/Common/Material.hlsl"
#include "Shaders/Common/Random.hlsl"
#include "Shaders/Common/Sampling.hlsl"
#include "Shaders/Common/Ray.hlsl"

#define PATH_SAMPLES 8
#define RECURSION_LENGTH 8

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

	uint PathSamples = 0;
	float3 Color = float3(0, 0, 0);

	float time = otherInfo.x;
	uint frameCount = asuint(otherInfo.y);

	uint2 randSeed = uint2(LaunchID.xy * time);

	// [POI]
	// This kind of loop is beneficial, because it bypasses the max recursion depth limit of the GPU which is set by the driver
	while(PathSamples < PATH_SAMPLES)
	{
		// Generate a random ray direction inside the pixel
		float2 rand = float2(NextRandomFloat(randSeed.x), NextRandomFloat(randSeed.y));
		const float2 pixelCenter = float2(LaunchID.xy) + rand;
		const float2 uv = pixelCenter / float2(LaunchSize.xy);

		RayDesc ray = ConstructRay(viewInverse, projInverse, uv);

		// Recurse trough the ray
		uint RecursionDepth = 0;
		while(RecursionDepth < RECURSION_LENGTH)
		{
			RecursionDepth++;

			// Trace ray with payload
			TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, p);
			// Update ray origin and direction that we got from the payload
			ray.Origin = p.RayInfo.Origin;
			ray.Direction = p.RayInfo.Direction;


			if(p.Info.TerminateRay)
				break;
		}
		
		Color += p.Info.Color * p.Info.LightContribution; // Add the color of the ray to the total color
		p.Info.Color = float3(1, 1, 1); // Reset the color for the next ray
		PathSamples++;
	}
	
	float3 Radiance = (Color) / float(PathSamples);

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


// [POI]
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

	float time = otherInfo.x; // Extract time from the uniform buffer
	// Use the time and the position of the hit to generate a random seed, so that the random numbers are different for each pixel
	uint seed = asint(time) * asint(v.x) * asint(v.y) * asint(v.z); 

	// Sample a new ray direction
	float2 rand = float2(NextRandomFloat(seed), NextRandomFloat(seed));

	// Sample a new ray direction
	float3 newDir = SampleCosineHemisphere(normal, rand);

	// angle between the normal and the new ray direction
	float cosTheta = dot(newDir, normal);

	// calculate the pdf of the new ray direction
	float pdf = CosineHemispherePDF(cosTheta);
	
	// This is the formiula for the Lambertian BRDF
	// float3 sampledColor = ((mat.BaseColor / PI) * cosTheta) / pdf; where pdf = cos(theta) / PI
	// The  above formula can be simplified to the following by basic algebra like this: 
	float3 sampledColor = mat.BaseColor;

		

	// Payload Update

	// if the material is emissive, terminate the ray and return the emissive color
	bool isLight = any(mat.Emissive);
	p.Info.LightContribution = isLight ? mat.Emissive: float3(0.0, 0.0, 0.0);
	// terminate the ray if the material is emissive
	p.Info.TerminateRay = isLight;
	// Add the color of the ray to the total color
	p.Info.Color *= sampledColor;

	// set the new ray direction and origin
	p.RayInfo.Direction = newDir;
	p.RayInfo.Origin = GetIntersectionPosition(); // Get the position of the hit
}


[shader("miss")]
void miss(inout Payload p)
{
	p.Info.Color = float3(0.0, 0.0, 0.0);
	p.Info.LightContribution = float3(0.0, 0.0, 0.0);
	p.Info.TerminateRay = true;
}

