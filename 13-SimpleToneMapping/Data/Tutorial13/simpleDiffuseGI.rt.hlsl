
// Include and import common Falcor utilities and data structures
__import Raytracing;
__import ShaderCommon; 
#include "HostDeviceData.h"            // Some #defines used for shading across Falcor
__import Shading;                      // Shading functions, etc   
__import Lights;                       // Light structures for our current scene

// A separate file with some simple utility functions: getPerpendicularVector(), initRand(), nextRand()
#include "simpleDiffuseGIUtils.hlsli"

// Include shader entries, data structures, and utility function to spawn shadow rays
#include "standardShadowRay.hlsli"

// A constant buffer we'll fill in for our ray generation shader
cbuffer RayGenCB
{
	float gMinT;
	uint  gFrameCount;
	bool  gDoIndirectGI;
}

// A constant buffer we'll fill in for our miss shader
cbuffer MissShaderCB
{
	bool    gUseLightProbe;
	float3  gBgColor;
	uint2   gEnvMapRes;
};

// Input and out textures that need to be set by the C++ code (for the ray gen shader)
Texture2D<float4> gPos;
Texture2D<float4> gNorm;
Texture2D<float4> gDiffuseMatl;
Texture2D<float4> gSpecMatl;
RWTexture2D<float4> gOutput;

// Input and out textures that need to be set by the C++ code (for the miss shader)
Texture2D<float4> gEnvMap;

struct IndirectRayPayload
{
	float3 color;
	int    pathLength;
	float  minT;
	uint   rndSeed;
};

[shader("miss")]
void IndirectMiss(inout IndirectRayPayload rayData : SV_RayPayload)
{
	if (gUseLightProbe)
	{
		// Convert our direction to a (u,v) coordinate
		float2 uv = wsVectorToLatLong(WorldRayDirection());

		// Lookup and return our light probe color
		rayData.color = gEnvMap[uint2(uv*gEnvMapRes)].rgb;
	}
	else
	{
		// Store the background color into our ray
		rayData.color = gBgColor;
	}
}

[shader("anyhit")]
void IndirectAnyHit(inout IndirectRayPayload rayData : SV_RayPayload, 
	                BuiltinIntersectionAttribs attribs : SV_IntersectionAttributes)
{
	// TODO: Handle alpha testing
}

[shader("closesthit")]
void IndirectClosestHit(inout IndirectRayPayload rayData : SV_RayPayload,
	                    BuiltinIntersectionAttribs attribs : SV_IntersectionAttributes)
{
	// Run a helper functions to extract Falcor scene data for shading
	ShadingData shadeData = getHitShadingData( attribs );

	// Which light are we randomly sampling?
	int lightToSample = min(int(nextRand(rayData.rndSeed) * gLightsCount), gLightsCount - 1);

	// Get our light information
	float distToLight;
	float3 lightIntensity;
	float3 toLight;
	getLightData(lightToSample, shadeData.posW, toLight, lightIntensity, distToLight);

	// Compute our lambertion term (L dot N)
	float LdotN = saturate(dot(shadeData.N, toLight));

	// Shoot our shadow ray.
	float shadowMult = float(gLightsCount) * shadowRayVisibility(shadeData.posW, toLight, gMinT, distToLight);

	// Return the color illuminated by this randomly selected light
	rayData.color = shadowMult * LdotN * lightIntensity * shadeData.diffuse / M_PI;
}


float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, uint curPathLen, uint seed)
{
	// Setup shadow ray
	RayDesc rayColor;
	rayColor.Origin = rayOrigin;
	rayColor.Direction = rayDir;
	rayColor.TMin = minT;
	rayColor.TMax = 1.0e38f;

	// Initialize the ray's payload data (with a distance > than the maximum possible)
	IndirectRayPayload payload;
	payload.color = float3(0, 0, 0);
	payload.pathLength = curPathLen + 1;
	payload.minT = minT;
	payload.rndSeed = seed;

	// Trace our ray
	TraceRay(gRtScene, 0, 0xFF, 1, hitProgramCount, 1, rayColor, payload);

	// Return the color we got from our ray
	return payload.color;
}

[shader("raygeneration")]
void SimpleDiffuseGIRayGen()
{
	// Where is this ray on screen?
	uint2 launchIndex = DispatchRaysIndex();
	uint2 launchDim   = DispatchRaysDimensions();

	// Load g-buffer data
	float4 worldPos     = gPos[launchIndex];
	float4 worldNorm    = gNorm[launchIndex];
	float4 difMatlColor = gDiffuseMatl[launchIndex];

	// We're only doing Lambertian, but sometimes Falcor gives a black Lambertian color.
	//    There, this shader uses the spec color for our Lambertian color.
	float4 specMatlColor = gSpecMatl[launchIndex];
	if (dot(difMatlColor.rgb, difMatlColor.rgb) < 0.00001f) difMatlColor = specMatlColor;

	// If we don't hit any geometry, our difuse material contains our background color.
	float3 shadeColor = difMatlColor.rgb;

	// Initialize our random number generator
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, gFrameCount, 16);

	// Our camera sees the background if worldPos.w is 0, only shoot an AO ray elsewhere
	if (worldPos.w != 0.0f)
	{
		// Which light are we samping?
		int lightToSample = min(int(nextRand(randSeed) * gLightsCount), gLightsCount - 1);

		// Get our light information
		float distToLight;
		float3 lightIntensity;
		float3 toLight;
		getLightData(lightToSample, worldPos.xyz, toLight, lightIntensity, distToLight);

		// Compute our lambertion term (L dot N)
		float LdotN = saturate(dot(worldNorm.xyz, toLight));

		// Shoot our ray.  Since we're randomly sampling lights, we have to increase their intensity based on
		//    probability of sampling.
		float shadowMult = float(gLightsCount) * shadowRayVisibility(worldPos.xyz, toLight, gMinT, distToLight);

		// Compute our Lambertian shading color
		shadeColor = shadowMult * LdotN * lightIntensity * difMatlColor.rgb / M_PI;

		if (gDoIndirectGI)
		{
			// Shoot a randomly selected diffuse interreflection ray
			float3 bounceDir = getCosHemisphereSample(randSeed, worldNorm.xyz);
			float3 bounceColor = shootIndirectRay(worldPos.xyz, bounceDir, gMinT, 0, randSeed);
			shadeColor += bounceColor * difMatlColor.rgb / M_PI;
		}
	}
	
	// Save out our AO color
	gOutput[launchIndex] = float4(shadeColor, 1.0f);
}
