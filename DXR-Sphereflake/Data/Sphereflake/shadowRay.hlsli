// This header is a self-contained header for shooting shadow rays.  Note it is not (quite)
//    generalizable to arbitrary programs, since it hard codes the hit group and miss shader
//    used when calling TraceRay()

// Our shadow ray's payload.  
struct ShadowPayload
{
	float shadow;
};

// Executed if we never hit an occluder, in which case the ray is *not* shadowed
[shader("miss")]
void ShadowRayMiss(inout ShadowPayload pay)
{
	pay.shadow = 1.0f;
}

float shootShadowRay(float3 origin, float3 direction)
{
	// Build our ray structure
	RayDesc   ray = { origin, gMinT, direction, 1.0e+38f };

	// Shadow payload.  We assume a ray *will* hit geometry.  If we don't hit geometry,
	//    our miss shader will set this payload value to 1.0.
	ShadowPayload pay = { 0.0f };

	// Trace our ray.  This is kind of complex.  The flags say we'll never run our closest-hit
	//    shader and as soon as we see any valid hit, we'll stop.  Because we never execute our
	//    closest hit shader, we use the same hit group (#0) as our color ray.  But we use the miss
	//    shader (#1) specific to our shadow rays.
	TraceRay(gRtScene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
		0xFF, 0, hitProgramCount, 1, ray, pay);

	// Return our shadow payload
	return pay.shadow;
}