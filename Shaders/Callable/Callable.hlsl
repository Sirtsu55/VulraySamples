
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

// [POI]
// Callable shaders require a custom data structure to be passed to them
struct CallableData
{
    float3 newColor;
};

[shader("closesthit")]
void chit(inout Payload p, in float2 attribs)
{
    // [POI]
    // we determine the callable shader to call based on the geometry index in th BLAS
    // This is either 0 or 1, because we have two geometries in the BLAS
	uint callIndex = GeometryIndex();
    
    CallableData callData;

    // [POI]
    // the first callable shader is call0, because the first callable shader in the SBT is call0 and then call1
    // we call the callable shader, pass the callable shader index and the data structure
    // Our SBT is structured as follows:
    // | rgen | ... | call0 | call1 | ... |
    CallShader(callIndex, callData);

    p.hitValue = callData.newColor;
}


bool Checkerboard(float size)
{
    uint3 dispatchIndex = DispatchRaysIndex();
    return (fmod(dispatchIndex.x, size) < size / 2.0) ^ (fmod(dispatchIndex.y, size) < size / 2.0);
}
[shader("callable")]
void call0(inout CallableData p)
{
    bool checker = Checkerboard(16.0);
    p.newColor = checker ? float3(1.0, 1.0, 1.0) : float3(1.0, 0.0, 0.0);
}
[shader("callable")]
void call1(inout CallableData p)
{
    bool checker = Checkerboard(16.0);
    p.newColor = checker ? float3(1.0, 1.0, 1.0) : float3(0.0, 0.0, 1.0);
}


[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue = float3(0.0, 0.0, 0.2);
}

