uint Random(uint v)
{
	uint state = v * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float3 HitAttribute(float3 vertexAttribute[3], in float2 attr)
{
    return vertexAttribute[0] +
        attr.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.y * (vertexAttribute[2] - vertexAttribute[0]);
}