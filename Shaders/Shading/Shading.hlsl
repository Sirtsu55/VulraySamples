#include "Shaders/Common/Material.hlsl"
#include "Shaders/Common/Functions.hlsl"

#define PATH_SAMPLES 4
#define RECURSION_LENGTH 2


// vk::binding(binding, set)
[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;
[[vk::binding(1, 0)]] cbuffer uniformBuffer 
{ 
	float4x4 viewInverse;
	float4x4 projInverse;
	float4 time; // time is in x
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
	const float2 inUV = pixelCenter/float2(LaunchSize.xy);
	float2 d = inUV * 2.0 - 1.0;
	float4 target = mul(projInverse, float4(d.x, d.y, 1, 1));

	RayDesc rayDesc;
	rayDesc.Origin = mul(viewInverse, float4(0,0,0,1)).xyz;
	rayDesc.Direction = mul(viewInverse, float4(normalize(target.xyz), 0)).xyz;
	rayDesc.TMin = 0.001;
	rayDesc.TMax = 10000.0;

	Payload payload;
	payload.hitValue = float4(0.0, 0.0, 0.0, 1.0);
	payload.lightContribution = float3(0.0, 0.0, 0.0);

	float3 AccumulatedColor = float3(0.0, 0.0, 0.0);

	int RecursionDepth = 0;
	float attenuation = 1.0;

	while (RecursionDepth < RECURSION_LENGTH)
	{
		RecursionDepth++;

		TraceRay(rs, RAY_FLAG_NONE, 0xFF, 0, 0, 0, rayDesc, payload);

		rayDesc.Direction = payload.newRayDirection;
		rayDesc.Origin = payload.hitPoint.xyz;

		if (payload.hitPoint.w < 0.0)
		{
			AccumulatedColor += payload.hitValue.xyz;
			break;
		}
		if (payload.hitValue.w < 0.01) // terminate ray if it's too dark
			break; 

		// atennuate color 
		AccumulatedColor += payload.hitValue.xyz * attenuation;
		// AccumulatedColor += float3(1,1,1) * attenuation;

		// update attenuation
		attenuation *= payload.hitValue.w;
	}
	
	float3 finalColor = AccumulatedColor * payload.lightContribution / float(RecursionDepth);

	image[int2(LaunchID.xy)] = float4(finalColor, 1.0);
	// image[int2(LaunchID.xy)] = time;
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

float3 SampleDirectionHemisphere(float3 v, float u1, float u2)
{
    float theta = atan2(0.5 * sqrt(u1), sqrt(1 - u1));
    float phi = 2 * PI * u2;

    float3 h;
    h.x = sin(theta) * cos(phi);
    h.y = sin(theta) * sin(phi);
    h.z = cos(theta);

    float3 sampleDir = 2.0f * dot(h, v) * h - v;
    return normalize(sampleDir);
}

[shader("closesthit")]
void chit(inout Payload p, in float2 barycentrics)
{
	uint matIndex = InstanceID() + GeometryIndex();

	GPUMaterial mat = materials[matIndex];

	float3 normals[3] = {
		GetNormal(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 0),
		GetNormal(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 1),
		GetNormal(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 2)
	};

	float3 normal = InterpolateTriangle(normals, barycentrics);
	
	float VdotN = dot(normal, WorldRayDirection());

	if(mat.Emissive.x > 0.0 || mat.Emissive.y > 0.0 || mat.Emissive.z > 0.0)
	{
		p.lightContribution = p.hitValue.xyz;
		p.hitPoint.w = -1.0; // terminate ray
	}
	else
	{
		p.lightContribution = float3(0.0, 0.0, 0.0);
		p.hitPoint.w = 1.0;
	}
	p.hitValue.xyz = mat.BaseColor;
	p.hitValue.w = SchlickApproximation(1.0, VdotN); // next ray will be attenuated by this amount

	uint rngState = asint(time.x);

	float r1 = NextFloat(rngState);
	float r2 = NextFloat(rngState);

	
	// p.newRayDirection = normalize(reflect(WorldRayDirection(), normal));
	p.hitPoint.xyz = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
}


[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue.xyz = float3(0.1, 0.7, 1.0);
	p.hitPoint = float4(0.0, 0.0, 0.0, -1.0);
	p.lightContribution = float3(1.0, 1.0, 1.0);
}

