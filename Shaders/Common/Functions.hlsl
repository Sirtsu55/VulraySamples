
#define PI 3.1415926535897932384626433832795
#define TWO_PI 6.283185307179586476925286766559
#define EULER_E 2.7182818284590452353602874713527


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

// Code from https://www.shadertoy.com/view/tlVczh
//----------------------------------------------------------------------
void GetOrthonormalBasis(in float3 n, out float3 xp, out float3 yp)
{
    
    float3 a = (abs(n.x) > 0.9) ? float3(0,1,0) : float3(1,0,0);
    yp = normalize(cross(n, a));
    xp = cross(n, yp);

}
//----------------------------------------------------------------------

float3 RotateOrthonormal(in float3 Normal, in float3 vectorToRotate)
{
    float3 tangent, binormal;
    GetOrthonormalBasis(Normal, tangent, binormal);

    return (tangent * vectorToRotate.x + binormal * vectorToRotate.y + Normal * vectorToRotate.z);
}

