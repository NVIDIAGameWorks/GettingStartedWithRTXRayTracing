
// This include assumes global shared variables of the following format have been declared:
//      shared Buffer<float4> gMatlData;

// Payload for our primary rays.  We really don't use this for this g-buffer pass
struct ColorPayload
{
	float3 color;
	uint depth;
	uint rngSeed;
};

// This shader determines the background color (i.e., what color is returned when a ray misses)
[shader("miss")]
void ColorRayMiss(inout ColorPayload pay)
{
	// A weird linear color ramp Pete uses in "Ray Tracing in One Weekend"
	float3 dir = normalize(WorldRayDirection());
	float t = 0.5*dir.y + 0.5f;
	pay.color = lerp(float3(1.0, 1.0, 1.0), float3(0.5, 0.7, 1.0), t);
}

// This is a utility function that sets up and traces a ray, then returns the resulting color
float3 shootColorRay(float3 origin, float3 direction, uint curDepth, uint seed)
{
	// Build our ray structure
	RayDesc ray = { origin, gMinT, direction, 1.0e+38f };

	// Build the payload for our color ray
	ColorPayload pay = { float3(0,0,0), curDepth + 1, seed }; 

	// Trace our ray.  Use hit group #0 and miss shader #0.
	TraceRay(gRtScene, RAY_FLAG_NONE, 0xFF, 0, hitProgramCount, 0, ray, pay);

	// Return the color from our ray payload
	return pay.color;
}