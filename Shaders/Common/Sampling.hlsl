
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

    T0 = abs(normal.x) < SQRT_OF_ONE_THIRD ? float3(1, 0, 0) : float3(0, 1, 0);

	// Use not-normal direction to generate two perpendicular directions
	T1 = normalize(cross(normal, T0));
	T2 = normalize(cross(normal, T1));
}
//----------------------------------------------------------------------

// Sample a hemisphere with cosine distribution
float3 SampleCosineHemisphere(float3 normal, float2 rand) 
{
    // Sample Spherical coordinates
    float theta = acos(sqrt(rand.x));
    float phi = TWO_PI * rand.y;

    // Get Orthonormal Bases
    float3 T1;
    float3 T2;
    GetOrthonormalBases(normal, T1, T2);

    // Convert to Cartesian coordinates
    float3 cartesian = SphericalToCartesian(theta, phi);

    // Transform the sample to the normal's coordinate system
    float3 sampleDir = cartesian.x * T1 + cartesian.y * T2 + cartesian.z * normal;
    
	return sampleDir;
}

// Returns the PDF for sampling a hemisphere with cosine distribution
float CosineHemispherePDF(float cosTheta)
{
    return cosTheta / PI;
}