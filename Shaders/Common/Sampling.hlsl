
#define PI 3.1415926535897932384626433832795
#define TWO_PI 6.283185307179586476925286766559
#define EULER_E 2.7182818284590452353602874713527
#define SQRT_OF_ONE_THIRD 0.57735026919


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
float SchlickFresnel(float specular, float angle)
{
    // Take the absolute value of the dot product to avoid negative values which can occur
    // if dotting a normal and view vector behind the surface
    return specular + (1.0f - specular) * pow(1.0f - angle, 5.0f);
}

//----------------------------------------------------------------------
void GetOrthonormalBases(in float3 normal, out float3 T1, out float3 T2)
{
    float3 T0 = float3(0, 0, 0);

    // adapted code, works the same way and less branching
    T0 = abs(normal.x) < SQRT_OF_ONE_THIRD ? float3(1, 0, 0) : float3(0, 1, 0);

	// Use not-normal direction to generate two perpendicular directions
	T1 = normalize(cross(normal, T0));
	T2 = normalize(cross(normal, T1));
}
//----------------------------------------------------------------------

float3 SampleCosineHemisphere(float3 normal, float u1, float u2) 
{

    float theta = acos(sqrt(u1));
    float phi = TWO_PI * u2;

    float3 perpendicularDirection1;
    float3 perpendicularDirection2;
    GetOrthonormalBases(normal, perpendicularDirection1, perpendicularDirection2);

    float3 cartesian = SphericalToCartesian(theta, phi);

    float3 sampleDir = cartesian.x * perpendicularDirection1 + cartesian.y * perpendicularDirection2 + cartesian.z * normal;

	return sampleDir;
}

float CosineHemispherePDF(float cosTheta)
{
    return cosTheta / PI;
}