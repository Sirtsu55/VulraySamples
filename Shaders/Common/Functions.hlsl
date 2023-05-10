
#define PI 3.1415926535897932384626433832795
#define TWO_PI 6.283185307179586476925286766559
#define EULER_E 2.7182818284590452353602874713527
#define SQRT_OF_ONE_THIRD 0.57735026919

// Xorshift random number generator
//----------------------------------------------------------------------
uint Random(uint state)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

uint NextRandomInt(inout uint seed)
{
    seed = Random(seed);
    return seed;
}
float NextRandomFloat(inout uint seed)
{
    seed = Random(seed);
    return seed / (float)0xffffffff;
}

//----------------------------------------------------------------------


float3 InterpolateTriangle(float3 vertexAttribute[3], in float2 barycentrics)
{
    return vertexAttribute[2] +
        barycentrics.x * (vertexAttribute[1] - vertexAttribute[2]) +
        barycentrics.y * (vertexAttribute[0] - vertexAttribute[2]); // barycentric interpolation
}

// Code from https://github.com/TheRealMJP/DXRPathTracer
// Explanation: https://computergraphics.stackexchange.com/a/4994 and https://agraphicsguy.wordpress.com/2015/11/01/sampling-microfacet-brdf/
//----------------------------------------------------------------------
float3 SampleDirectionGGX(float3 v, float3 n, float roughness, float u1, float u2)
{
    float theta = atan2(roughness * sqrt(u1), sqrt(1 - u1));
    float phi = 2 * PI * u2;

    float3 h;
    h.x = sin(theta) * cos(phi);
    h.y = sin(theta) * sin(phi);
    h.z = cos(theta);

    float3 sampleDir = 2.0f * dot(h, v) * h - v;
    return normalize(sampleDir);
}
//----------------------------------------------------------------------





// Computes the Fresnel term for a dielectric material using Schlick's approximation
// Specular is the specular intensity
float SchlickApproximation(float specular, float angle)
{
    // Take the absolute value of the dot product to avoid negative values which can occur
    // if dotting a normal and view vector behind the surface
    float dotNV = abs(angle);
    return specular + (1.0f - specular) * pow(1.0f - dotNV, 5.0f);
}

float3 SchlickApproximation(float3 specularColor, float angle)
{
    return specularColor + (1.0f - specularColor) * pow(1.0f - angle, 5.0f);
}

// Code from https://github.com/rtx-on/rtx-explore, Directory = src/D3D12PathTracer/src/shaders/Raytracing.hlsl
//----------------------------------------------------------------------
float3 CalculateRandomDirectionInHemisphere(float3 normal, float roughness, float u1, float u2) {

	float up = sqrt(u1); // cos(theta)
	float over = sqrt(1 - up * up); // sin(theta)
	float around = u2 * TWO_PI;


    // float theta = atan2(roughness * sqrt(u1), sqrt(1 - u1));
    // float phi = TWO_PI * u2;

    // float3 v = WorldRayDirection();

    // float3 h;
    // h.x = sin(theta) * cos(phi);
    // h.y = sin(theta) * sin(phi);
    // h.z = cos(theta);

    // float3 sampleDir = 2.0f * dot(h, v) * h - v;

    float3 directionNotNormal = abs(normal.x) < SQRT_OF_ONE_THIRD ? float3(1, 0, 0) : float3(0, 0, 1);

	// if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
	// 	directionNotNormal = float3(1, 0, 0);
	// }
	// else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
	// 	directionNotNormal = float3(0, 1, 0);
	// }
	// else {
	// 	directionNotNormal = float3(0, 0, 1);
	// }

	// Use not-normal direction to generate two perpendicular directions
	float3 perpendicularDirection1 =
		normalize(cross(normal, directionNotNormal));
	float3 perpendicularDirection2 =
		normalize(cross(normal, perpendicularDirection1));

    // return sampleDir.x * perpendicularDirection1
    //     + sampleDir.y * perpendicularDirection2
    //     + sampleDir.z * normal;

	return up * normal
		+ cos(around) * over * perpendicularDirection1
		+ sin(around) * over * perpendicularDirection2;
}
//----------------------------------------------------------------------