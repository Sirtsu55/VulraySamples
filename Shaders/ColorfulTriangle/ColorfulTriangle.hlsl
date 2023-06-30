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

	// start with screen space coordinates
	const float2 pixelCenter = float2(LaunchID.xy) + float2(0.5, 0.5);
	const float2 inUV = pixelCenter/float2(LaunchSize.xy);
	float2 d = inUV * 2.0 - 1.0; // From [-1, -1] to [1, 1]
	float4 target = mul(projInverse, float4(d.x, d.y, 1, 1)); // From clip space to view space

	RayDesc rayDesc;
	rayDesc.Origin = mul(viewInverse, float4(0,0,0,1)).xyz; // extract position from view matrix
	rayDesc.Direction = mul(viewInverse, float4(normalize(target.xyz), 0)).xyz; // Transform to world space
	rayDesc.TMin = 0.001;
	rayDesc.TMax = 10000.0;

	Payload payload;
	TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);

	image[int2(LaunchID.xy)] = float4(payload.hitValue, 0.0);
}




[shader("closesthit")]
void chit(inout Payload p, in float2 attribs)
{
  const float3 barycentricCoords = float3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  p.hitValue = barycentricCoords;
}


[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue = float3(0.0, 0.0, 0.2);
}