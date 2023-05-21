
// includes are relative to the executable directory
#include "Shaders/Common/Material.hlsl"
#include "Shaders/Common/Ray.hlsl"

// vk::binding(binding, set)
[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;
[[vk::binding(1, 0)]] cbuffer uniformBuffer 
{ 
	float4x4 viewInverse;
	float4x4 projInverse;
	float4 otherInfo; // time is in x
};
[[vk::binding(2, 0)]] RWTexture2D<float4> image;
[[vk::binding(3, 0)]] RWStructuredBuffer<GPUMaterial> materials;


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




[shader("closesthit")]
void chit(inout Payload p, in float2 attribs)
{
	// Get the material index from the geometry index
	// IndexID() is the index that we set when creating the TLAS instance
	// GeometryIndex() is the index of the geometry in the BLAS
	// For the Materials Sample the V-shaped geometry is the first geometry in the BLAS
	// and InstanceID() was set to 0, so the material index is 0 + 0 = 0
	// Visual representation of the Material buffer:
	


	uint matIndex = InstanceID() + GeometryIndex();

	GPUMaterial mat = materials[matIndex];

  	p.hitValue = mat.BaseColor;

	
}


[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue = float3(0.0, 0.0, 0.2);
}

