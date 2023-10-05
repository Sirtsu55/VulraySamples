#include "Shaders/Common/Ray.hlsl"

// vk::binding(binding, set)
[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;
[[vk::binding(1, 0)]] cbuffer uniformBuffer 
{ 
	float4x4 viewInverse;
	float4x4 projInverse;
	float4 otherInfo; 
};
[[vk::binding(2, 0)]] RWTexture2D<float4> image;

struct Payload
{
[[vk::location(0)]] float3 hitValue;
};

[shader("raygeneration")]
void rgen()
{
	uint3 LaunchID = DispatchRaysIndex();
	uint3 LaunchSize = DispatchRaysDimensions();

	const float2 pixelCenter = float2(LaunchID.xy) + float2(0.5, 0.5);
	const float2 uv = pixelCenter/float2(LaunchSize.xy);

	// Abstract the construction of the ray from the camera, look at Camera.hlsl for details
	RayDesc rayDesc = ConstructRay(viewInverse, projInverse, uv);

	Payload payload;
	TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);

	image[int2(LaunchID.xy)] = float4(payload.hitValue, 0.0);
}

// [POI] This is how we can access the SBT data from the shader
[[vk::shader_record_ext]] cbuffer hitRecord { float3 triangleColor; }
[shader("closesthit")]
void chit(inout Payload p, in BuiltInTriangleIntersectionAttributes attribs)
{
    p.hitValue = triangleColor;
}


[[vk::shader_record_ext]] cbuffer missRecord { float3 backgroundColor; }
[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue = backgroundColor;
}

