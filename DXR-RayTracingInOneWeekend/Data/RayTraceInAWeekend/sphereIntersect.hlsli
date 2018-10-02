// This header includes a simple DXR sphere intersection routine.  The computed
//      sphere attributes are not well thought out -- returning the sphere center
//      is probably not the ideal return value, but it was simple and provided me
//      me with everything I needed for this simple demo

// This include assumes global shared variables of the following format have been declared:
//      shared Buffer<float3> gAABBData;  

// The attributes our sphere intersection returns to hit shaders
struct SphereAttribs
{
	float3 sphereCtr;
};

[shader("intersection")]
void SphereIntersect()
{
	// Get data about the sphere
	uint sphereNum = PrimitiveIndex();
	float3 boxMin = gAABBData[2 * sphereNum].rgb;
	float3 boxMax = gAABBData[2 * sphereNum + 1].rgb;
	float3 center = (boxMin + boxMax) * 0.5f;
	float  radius = abs((boxMax.x - boxMin.x) * 0.5f);

	// Get data about the ray
	float3 orig = WorldRayOrigin();
	float3 dir = WorldRayDirection();

	// Compute a, b, c, for quadratic in ray-sphere intersection
	//    (Math can be simplified; left in its entirety for clarity.)
	float3 toCtr = orig - center;
	float a = dot(dir, dir);
	float b = 2.0f*dot(dir, toCtr);
	float c = dot(toCtr, toCtr) - radius*radius;

	// Checks if sqrt(b^2 - 4*a*c) in quadratic equation has real answer
	float discriminant = b * b - 4.0f*a*c;
	if (discriminant >= 0.0f)
	{
		float sqrtVal = sqrt(discriminant);
		SphereAttribs sphrAttr = { center };
		//int stable = 1;
		//if (stable != 0) {
			// from Press, William H., Saul A. Teukolsky, William T. Vetterling, and Brian P. Flannery, 
			// "Numerical Recipes in C," Cambridge University Press, 1992.
			float q = (b >= 0) ? (-0.5*(b + sqrtVal)) : (-0.5*(b - sqrtVal));

			// we don't bother testing for division by zero
			ReportHit(q/a, 0, sphrAttr);
			ReportHit(c/q, 0, sphrAttr);
		//}
		//else {
		//	ReportHit((-b - sqrtVal) / (2.0f * a), 0, sphrAttr);
		//	ReportHit((-b + sqrtVal) / (2.0f * a), 0, sphrAttr);
		//}
	}
}

