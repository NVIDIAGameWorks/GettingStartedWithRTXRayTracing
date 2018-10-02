// Payload for our primary rays.  We really don't use this for this g-buffer pass
struct ShadowRayPayload
{
	int hitDist;
};


// Standard shadow rays shaders
[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayData : SV_RayPayload)
{
}

[shader("anyhit")]
void ShadowAnyHit(inout ShadowRayPayload rayData : SV_RayPayload, BuiltinIntersectionAttribs attribs : SV_IntersectionAttributes)
{
	rayData.hitDist = RayTCurrent();
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload rayData : SV_RayPayload, BuiltinIntersectionAttribs attribs : SV_IntersectionAttributes)
{
	rayData.hitDist = RayTCurrent();
}


// A utility function to trace a shadow ray and return 1 if no shadow and 0 if shadowed.
//    -> Note:  This assumes the shadow hit programs and miss programs are index 0!
float shadowRayVisibility(float3 origin, float3 direction, float minT, float maxT)
{
	// Setup our shadow ray
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = minT;
	ray.TMax = maxT;

	// Query if anything is between the current point and the light (i.e., at maxT) 
	ShadowRayPayload rayPayload = { maxT + 1.0f };
	TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, hitProgramCount, 0, ray, rayPayload);

	// Check if anyone was closer than our maxT distance (in which case we're occluded)
	return (rayPayload.hitDist > maxT) ? 1.0f : 0.0f;
}