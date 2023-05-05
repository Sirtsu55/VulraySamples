
#define PI 3.1415926535897932384626433832795
#define EULER_E 2.7182818284590452353602874713527


// Code from https://www.shadertoy.com/view/tsf3Dn
//----------------------------------------------------------------------
int Random(int value) {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

int NextInt(inout int seed) {
    seed = Random(seed);
    return seed;
}

float NextFloat(inout int seed) {
    seed = Random(seed);
    return abs(frac(float(seed) / 3141.592653));
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
// Specular Color is the color of the specular reflection
float3 SchlickApproximation(float3 specularColor, float angle)
{
    float dotNV = abs(angle);
    return specularColor + (1.0f - specularColor) * pow(1.0f - dotNV, 5.0f);
}

// Computes the Fresnel term for a dielectric material using Schlick's approximation
// Specular is the specular intensity
float SchlickApproximation(float specular, float angle)
{
    float dotNV = abs(angle); 
    return specular + (1.0f - specular) * pow(1.0f - dotNV, 5.0f);
}