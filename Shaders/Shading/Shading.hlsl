#include "Shaders/Common/Camera.hlsl"
#include "Shaders/Common/Material.hlsl"
#include "Shaders/Common/Functions.hlsl"

#define PATH_SAMPLES 16

// vk::binding(binding, set)
[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;
[[vk::binding(1, 0)]] cbuffer cam { CameraProperties cam; };
[[vk::binding(2, 0)]] RWTexture2D<float4> image;
[[vk::binding(3, 0)]] RWStructuredBuffer<GPUMaterial> materials;
[[vk::binding(4, 0)]] StructuredBuffer<Vertex> VertexBuffer;
[[vk::binding(5, 0)]] StructuredBuffer<uint> IndexBuffer;

struct Payload
{
[[vk::location(0)]] float3 hitValue;
[[vk::location(1)]] float3 hitNormal;
[[vk::location(2)]] float3 hitPoint;
};

[shader("raygeneration")]
void rgen()
{
	uint3 LaunchID = DispatchRaysIndex();
	uint3 LaunchSize = DispatchRaysDimensions();

	const float2 pixelCenter = float2(LaunchID.xy) + float2(0.5, 0.5);
	const float2 inUV = pixelCenter/float2(LaunchSize.xy);
	float2 d = inUV * 2.0 - 1.0;
	float4 target = mul(cam.projInverse, float4(d.x, d.y, 1, 1));

	RayDesc rayDesc;
	rayDesc.Origin = mul(cam.viewInverse, float4(0,0,0,1)).xyz;
	rayDesc.Direction = mul(cam.viewInverse, float4(normalize(target.xyz), 0)).xyz;
	rayDesc.TMin = 0.001;
	rayDesc.TMax = 10000.0;

	Payload payload;

	float3 accum = float3(0.0, 0.0, 0.0);

	int samples = 0;
	for (; samples < PATH_SAMPLES; samples++)
	{
		TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);
		rayDesc.Origin = payload.hitPoint;

		float rand1 = Random(inUV.x);
		float rand2 = Random(inUV.y);
		float rand3 = Random(rayDesc.Origin.x);

		float3 dir = payload.hitNormal;

		rayDesc.Direction = normalize(dir + float3(rand1, rand2, rand3) * 0.6);

		accum += payload.hitValue;
		if(payload.hitPoint.x == 0.0 && payload.hitPoint.y == 0.0 && payload.hitPoint.z == 0.0)
			break;
	}
	samples = samples == 0 ? 1 : samples;
	float3 finalColor = saturate(accum / float(samples));
	image[int2(LaunchID.xy)] = float4(finalColor, 0.0);
}

float3 GetVertex(in uint vertBufferStart, in uint indexBufferStart, in uint index)
{
    uint idx = IndexBuffer[indexBufferStart + index];
	return VertexBuffer[vertBufferStart + idx].Position.xyz;
}



[shader("closesthit")]
void chit(inout Payload p, in float2 attribs)
{
	// Get the material index from the geometry index
	// IndexID() is the index that we set when creating the TLAS instance
	// GeometryIndex() is the index of the geometry in the BLAS
	// For the Materials Sample the V-shaped geometry is the first geometry in the BLAS
	// and InstanceID() was set to 0, so the material index is 0 + 0 = 0

	uint matIndex = InstanceID() + GeometryIndex();

	GPUMaterial mat = materials[matIndex];

	float3 t0 = GetVertex(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 0);
	float3 t1 = GetVertex(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 1);
	float3 t2 = GetVertex(mat.VertBufferStart, mat.IndexBufferStart, PrimitiveIndex() * 3 + 2);
	float3 normal = normalize(cross(t1 - t0, t2 - t0));
	p.hitValue = mat.BaseColor;
	p.hitNormal = normal;
	p.hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
}


[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue = float3(0.0, 0.0, 0.2);
	p.hitPoint = float3(0.0, 0.0, 0.0);
}

