// This file contains a number of shading utilities, including a generalized refract() function,
//     some texture lookup utilities, along with functions that extract information and
//     shade our spheres based on our custom (and quite simplistic) material format


// Get a refraction ray giving a viewing ray (i.e., towards the surface), a surface normal, and ratio of indices of refraction
float3 getRefractionDir(float3 V, float3 N, float IoR)
{
	// If the normal is pointing the other way, reverse it, since we expect N dot V <= 0
	float3 norm = dot(N, V) <= 0.0f ? N : -N;

	// Compute refraction ray
	float NdotV = -dot(norm, V);
	float disc = 1.0f - IoR*IoR*(1 - NdotV*NdotV);
	float3 refr = IoR * V + (IoR * NdotV - sqrt(disc)) * norm;

	// If refraction was valid, return that, otherwise return a total internal reflection ray
	return (disc > 0.0f) ? refr : reflect(V, norm);
}

// A work-around for an apparent compiler bug in atan2() on some NVIDIA drivers
float atan2_WAR(float y, float x)
{
	if (x > 0.f)
		return atan(y / x);
	else if (x < 0.f && y >= 0.f)
		return atan(y / x) + M_PI;
	else if (x < 0.f && y < 0.f)
		return atan(y / x) - M_PI;
	else if (x == 0.f && y > 0.f)
		return M_PI / 2.f;
	else if (x == 0.f && y < 0.f)
		return -M_PI / 2.f;
	return 0.f; // x==0 && y==0 (undefined)
}

// Converts a direction to a lat-long 
float2 wsVectorToLatLong(float3 dir)
{
	float3 p = normalize(dir);

	// atan2_WAR is a work-around due to an apparent compiler bug in atan2
	float u = (1.f + atan2_WAR(p.x, -p.z) / M_PI) * 0.5f;
	float v = acos(p.y) / M_PI;
	return float2(u, v);
}

// Pass in a texture that is a lat-long parameterized spherical texture, a direction, and a random rotation
float3 lookupInLatLongTexture(Texture2D<float4> texMap, float3 normal, float randOffset)
{
	float2 dims;
	texMap.GetDimensions(dims.x, dims.y);
	float2 uv = frac(wsVectorToLatLong(normal) + float2(randOffset, 0));
	uv.x = 1.0f - uv.x;
	return texMap[uint2(uv * dims)].rgb;
}


// Queries if the current material is diffuse
bool isDiffuseMaterial(float4 matlData)
{
	return (matlData.a == clamp(matlData.a, 0.0f, 1.5f));
}

// Queries if the current material is metal
bool isMetalMaterial(float4 matlData)
{
	return (matlData.a == clamp(matlData.a, 2.0f, 4.0f));
}

// Queries if the current material is glass
bool isGlassMaterial(float4 matlData)
{
	return (matlData.a == clamp(matlData.a, 5.0f, 6.0f));
}

float3 shadeDiffuseMaterial(float4 matlData, float3 hit, float3 V, float3 N, inout uint randSeed, float area, float procTxr, float hemi)
{
	float isVisible;

	// set to 1 to use cosine-weighted hemispherical dome lighting, *very* vaguely like the sky
	if (hemi != 0.0f) {
		// Pick a random ray on the hemisphere, shoot a shadow ray in that direction.
		float3 randDir = getCosHemisphereSample(randSeed, N);
		isVisible = shootShadowRay(hit + N * 0.001f, randDir);
	}
	else {
		// three directional light sources
		float3 light1 = float3(-0.507093, 0.845154, -0.169031);
		float3 light2 = float3( 0.174078, 0.696311,  0.696311);
		float3 light3 = float3( 0.742781, 0.371391, -0.557086);
		// relative size of area light measured perpendicular to ray direction.
		light1 = getCircularAreaSample(randSeed, light1, area);
		light2 = getCircularAreaSample(randSeed, light2, area);
		light3 = getCircularAreaSample(randSeed, light3, area);

		// Shadows: Move a bit along ray towards each light to avoid self-intersection.
		//     epsilon is relatively high for this scene, because diffuse ground plane is a giant sphere and so has precision problems
		float eps = 0.0001;
		// less accurate, but safer, is to use N instead of light1/2/3 for moving the ray's origin, to ensure it is above the surface.
		isVisible = 0.5f * 
			( shootShadowRay(hit + N * eps, light1) * saturate(dot(N, light1))
			+ shootShadowRay(hit + N * eps, light2) * saturate(dot(N, light2))
			+ shootShadowRay(hit + N * eps, light3) * saturate(dot(N, light3)) );
	}

	float3 matlColor;
	if (procTxr != 0.0f) {
		// Use the reflective properties in our material structure...
		//     the 1000 is to keep the values positive so that fmod works for negative numbers;
		//     is there a better way?
		float scalePat = 0.1;
		if (fmod(hit.x + 1000, scalePat*0.5) > scalePat*0.25) {
			if (fmod(hit.z + 1000, scalePat*0.5) > scalePat*0.25) {
				matlColor = matlData.rgb;
			}
			else {
				matlColor = matlData.gbr;
			}
		}
		else {
			if (fmod(hit.z + 1000, scalePat*0.5) > scalePat*0.25) {
				matlColor = matlData.rbg;
			}
			else {
				matlColor = matlData.bgr;
			}
		}
	}
	else {
		matlColor = matlData.rgb;
	}

	// Return our visibility (scaled by each light's effect) modulated by the selected diffuse color
	return isVisible * matlColor;
}

float3 shadeMetalMaterial(float4 matlData, float3 hit, float3 V, float3 N, uint rayDepth, inout uint randSeed)
{
	// Is this surface glossy?  If so, compute a glossy perturbation vector
	float perturbRadius = frac(saturate(matlData.a - 2.0f));  // Gives 0 if matlData.a <= 2 or matlData >= 3
	//float3 perturb      = perturbRadius * float3(nextRand(randSeed), nextRand(randSeed), nextRand(randSeed));

	// Are we applying normal mapping?  Check the material properties and the global UI flag.
	//float3 surfNorm = N;
	
	// Compute our reflection vector and add the random variation
	//float3 reflDir = reflect(V, surfNorm) + perturb;
	float3 reflDir = reflect(V, N);

	// If the perturbed ray is above the horizon, shoot it and return the appropriate color
	//if (dot(reflDir, surfNorm) > 0.0f)
	//{
		// move ray off of surface by N to avoid self reflection; using reflDir, commented out below, would be more accurate, but
		// has a risk of not moving the ray origin far enough from the surface:
		return matlData.rgb * shootColorRay(hit + N * 0.00001, reflDir, rayDepth, randSeed);
		// moving along the reflection direction will be more accurate; fancier still, not done here, is
		// to move the ray a distance/(dot(reflDir,N)), with some maximum movement, sort of like z-fighting.
		//return matlData.rgb * shootColorRay(hit + reflDir * 0.00001, reflDir, rayDepth, randSeed);
	//}

	// If our perturbed ray is below the horizon, it's absorbed; return black.
	//return float3(0, 0, 0);
}

float3 shadeGlassMaterial(float4 matlData, float3 hit, float3 V, float3 N, uint rayDepth, inout uint randSeed)
{
	// When refracting, the important thing is the ratio of indices of refraction.  
	//    * matlData.r contains the IoR for the glass ball.  We assume the other side is air (with IoR of 1.0f)
	//    * The _ratio_ depends on if we're entering or leaving the sphere.  
	//    * Entering the sphere, we want IoR_air / IoR_sphere.  Exiting, we want IoR_sphere / IoR_air
	//    * We enter the sphere when N dot V <= 0.0f, since we know our normals always point out.
	float curIor = dot(N, V) <= 0.0f ? 1.0f / matlData.r : matlData.r;

	// Are we randomly perturbing our refraction rays to add a frosted glass look?
	float perturbRadius = gPerturbRefractions ? saturate(matlData.g) : 0.0f;
	float3 perturb = perturbRadius * float3(nextRand(randSeed), nextRand(randSeed), nextRand(randSeed));

	// Get our refraction (or total internal reflection) direction
	float3 refrDir = getRefractionDir(V, N, curIor);

	// When we perturb our vector, it might go below the horizon.  In that case, we don't send a ray.
	if (dot(refrDir, N) * dot(refrDir + perturb, N) < 0.0f)
		return float3(0, 0, 0);

	// Shoot our refraction ray
	return shootColorRay(hit, normalize(refrDir + perturb), rayDepth, randSeed);
}