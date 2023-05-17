

RayDesc ConstructRay(in float4x4 viewInverse, in float4x4 projInverse, in float2 inUV)
{
	float2 d = inUV * 2.0 - 1.0;
	float4 target = mul(projInverse, float4(d.x, d.y, 1, 1));

    RayDesc rayDesc;
	rayDesc.Origin = mul(viewInverse, float4(0,0,0,1)).xyz;
	rayDesc.Direction = mul(viewInverse, float4(normalize(target.xyz), 0)).xyz;
	rayDesc.TMin = 0.001;
	rayDesc.TMax = 10000.0;
    return rayDesc;
}