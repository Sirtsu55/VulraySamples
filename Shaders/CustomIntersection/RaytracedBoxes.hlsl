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
	const float2 inUV = pixelCenter/float2(LaunchSize.xy);
	float2 d = inUV * 2.0 - 1.0;
	float4 target = mul(projInverse, float4(d.x, d.y, 1, 1));

	RayDesc rayDesc;
	rayDesc.Origin = mul(viewInverse, float4(0,0,0,1)).xyz;
	rayDesc.Direction = mul(viewInverse, float4(normalize(target.xyz), 0)).xyz;
	rayDesc.TMin = 0.001;
	rayDesc.TMax = 10000.0;

	Payload payload;
	TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);

	image[int2(LaunchID.xy)] = float4(payload.hitValue, 0.0);
}

// Custom hit attributes used to pass data from intersection shader to closest hit shader
// eg. Triangles have barycentric coordinates (a, b, c) as hit attributes (built in to Vulkan/DXR)
struct BoxHitAttributes
{
	float2 hitValue;
};

// Intersection shader for a box, This can be used for any geometry (boxes, spheres, cylinders, etc.), but it's not very efficient 
// due to the lack of hardware acceleration
[shader("intersection")]
void isect()
{
	BoxHitAttributes attribs;
	attribs.hitValue = float2(1.0, 1.0); // fill in the hit attributes, with whatever you want to pass to the closest hit shader

	float tHit = RayTCurrent(); // Get the current t value of the ray, when it hit the geometry

	// Report the hit: 1. t value, 2. hit kind (User defined unsigned int 0 - 127), and the custom hit attributes
    ReportHit(tHit, 0, attribs); 
}


[shader("closesthit")]
void chit(inout Payload p, in BoxHitAttributes attribs)
{
	// shade the pixel with the hit attributes, we got from the intersection shader
  	p.hitValue = float3(attribs.hitValue, 0.0);
}


[shader("miss")]
void miss(inout Payload p)
{
    p.hitValue = float3(0.0, 0.0, 0.2);
}