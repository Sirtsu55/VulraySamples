
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


float3 SphericalToCartesian(float theta, float phi)
{
    return float3(
        cos(phi) * sin(theta),
        sin(phi) * sin(theta),
        cos(theta)
    );
}



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

void GetOrthonormalBases(in float3 normal, out float3 perpendicularDirection1, out float3 perpendicularDirection2)
{
    // Original code from GitHub
	// if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
	// 	directionNotNormal = float3(1, 0, 0);
	// }
	// else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
	// 	directionNotNormal = float3(0, 1, 0);
	// }
	// else {
	// 	directionNotNormal = float3(0, 0, 1);
	// }

    // adapted code, works the same way and less branching
    float3 directionNotNormal = abs(normal.x) < SQRT_OF_ONE_THIRD ? float3(1, 0, 0) : float3(0, 1, 0);

	// Use not-normal direction to generate two perpendicular directions
	perpendicularDirection1 = normalize(cross(normal, directionNotNormal));
	perpendicularDirection2 = normalize(cross(normal, perpendicularDirection1));
}

//----------------------------------------------------------------------
float3 CalculateRandomDirectionInHemisphere(float3 normal, float roughness, float u1, float u2) 
{
    
    float theta = acos(u1);
    float phi = TWO_PI * u2;
    
	float z = sqrt(u1); // cos(theta)
	float y = sqrt(1 - z * z); // sin(theta)
	float x = u2 * TWO_PI;

    float3 perpendicularDirection1;
    float3 perpendicularDirection2;

    GetOrthonormalBases(normal, perpendicularDirection1, perpendicularDirection2);

    float3 cartesian = SphericalToCartesian(theta, phi);


    float3 sampleDir = cartesian.x * perpendicularDirection1 + cartesian.y * perpendicularDirection2 + cartesian.z * normal;

	return sampleDir;
}

