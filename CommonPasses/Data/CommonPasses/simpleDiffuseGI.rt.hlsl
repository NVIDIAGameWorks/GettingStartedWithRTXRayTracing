/**********************************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#  * Redistributions of code must retain the copyright notice, this list of conditions and the following disclaimer.
#  * Neither the name of NVIDIA CORPORATION nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT
# SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

// Some shared Falcor stuff for talking between CPU and GPU code
#include "HostDeviceSharedMacros.h"
#include "HostDeviceData.h"           

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

// A separate file with some simple utility functions: getPerpendicularVector(), initRand(), nextRand()
#include "simpleDiffuseGIUtils.hlsli"

// Include shader entries, data structures, and utility function to spawn shadow rays
#include "standardShadowRay.hlsli"

// A constant buffer we'll populate from our C++ code  (used for our ray generation shader)
cbuffer RayGenCB
{
	float gMinT;           // Min distance to start a ray to avoid self-occlusion
	uint  gFrameCount;     // An integer changing every frame to update the random number
	bool  gDoIndirectGI;   // A boolean determining if we should shoot indirect GI rays
	bool  gCosSampling;    // Use cosine sampling (true) or uniform sampling (false)
}

// Input and out textures that need to be set by the C++ code (for the ray gen shader)
Texture2D<float4> gPos;
Texture2D<float4> gNorm;
Texture2D<float4> gDiffuseMatl;
RWTexture2D<float4> gOutput;

// The payload used for our indirect global illumination rays
struct IndirectRayPayload
{
	float3 color;    // The (returned) color in the ray's direction
	uint   rndSeed;  // Our random seed, so we pick uncorrelated RNGs along our ray
};

// Our environment map, used for the miss shader for indirect rays
Texture2D<float4> gEnvMap;

// What code is executed when our ray misses all geometry?
[shader("miss")]
void IndirectMiss(inout IndirectRayPayload rayData)
{
	// Load some information about our lightprobe texture
	float2 dims;
	gEnvMap.GetDimensions(dims.x, dims.y);

	// Convert our ray direction to a (u,v) coordinate
	float2 uv = wsVectorToLatLong(WorldRayDirection());

	// Load our background color, then store it into our ray payload
	rayData.color = gEnvMap[uint2(uv * dims)].rgb;
}

[shader("anyhit")]
void IndirectAnyHit(inout IndirectRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Is this a transparent part of the surface?  If so, ignore this hit
	if (alphaTestFails(attribs))
		IgnoreHit();
}

// What code is executed when we have a new closest hitpoint?   Well, pick a random light,
//    shoot a shadow ray to that light, and shade using diffuse shading.
[shader("closesthit")]
void IndirectClosestHit(inout IndirectRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Run a helper functions to extract Falcor scene data for shading
	ShadingData shadeData = getHitShadingData( attribs );

	// Pick a random light from our scene to shoot a shadow ray towards
	int lightToSample = min(int(nextRand(rayData.rndSeed) * gLightsCount), gLightsCount - 1);

	// Query the scene to find info about the randomly selected light
	float distToLight;
	float3 lightIntensity;
	float3 toLight;
	getLightData(lightToSample, shadeData.posW, toLight, lightIntensity, distToLight);

	// Compute our lambertion term (L dot N)
	float LdotN = saturate(dot(shadeData.N, toLight));

	// Shoot our shadow ray to our randomly selected light
	float shadowMult = float(gLightsCount) * shadowRayVisibility(shadeData.posW, toLight, RayTMin(), distToLight);

	// Return the Lambertian shading color using the physically based Lambertian term (albedo / pi)
	rayData.color = shadowMult * LdotN * lightIntensity * shadeData.diffuse / M_PI;
}

// A utility function to trace an idirect ray and return the color it sees.
//    -> Note:  This assumes the indirect hit programs and miss programs are index 1!
float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, uint seed)
{
	// Setup shadow ray
	RayDesc rayColor;
	rayColor.Origin = rayOrigin;  // Where does it start?
	rayColor.Direction = rayDir;  // What direction do we shoot it?
	rayColor.TMin = minT;         // The closest distance we'll count as a hit
	rayColor.TMax = 1.0e38f;      // The farthest distance we'll count as a hit

	// Initialize the ray's payload data with black return color and the current rng seed
	IndirectRayPayload payload;
	payload.color = float3(0, 0, 0);  
	payload.rndSeed = seed;

	// Trace our ray to get a color in the indirect direction.  Use hit group #1 and miss shader #1
	TraceRay(gRtScene, 0, 0xFF, 1, hitProgramCount, 1, rayColor, payload);

	// Return the color we got from our ray
	return payload.color;
}

[shader("raygeneration")]
void SimpleDiffuseGIRayGen()
{
	// Where is this ray on screen?
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim   = DispatchRaysDimensions().xy;

	// Load g-buffer data
	float4 worldPos     = gPos[launchIndex];
	float4 worldNorm    = gNorm[launchIndex];
	float4 difMatlColor = gDiffuseMatl[launchIndex];

	// If we don't hit any geometry, our difuse material contains our background color.
	float3 shadeColor = difMatlColor.rgb;

	// Initialize our random number generator
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, gFrameCount, 16);

	// Our camera sees the background if worldPos.w is 0, only do diffuse shading & GI elsewhere
	if (worldPos.w != 0.0f)
	{
		// Pick a random light from our scene to sample for direct lighting
		int lightToSample = min(int(nextRand(randSeed) * gLightsCount), gLightsCount - 1);

		// We need to query our scene to find info about the current light
		float distToLight;
		float3 lightIntensity;
		float3 toLight;
		getLightData(lightToSample, worldPos.xyz, toLight, lightIntensity, distToLight);

		// Compute our lambertion term (L dot N)
		float LdotN = saturate(dot(worldNorm.xyz, toLight));

		// Shoot our ray for our direct lighting
		float shadowMult = float(gLightsCount) * shadowRayVisibility(worldPos.xyz, toLight, gMinT, distToLight);

		// Compute our Lambertian shading color using the physically based Lambertian term (albedo / pi)
		shadeColor = shadowMult * LdotN * lightIntensity * difMatlColor.rgb / M_PI;

		// Now do our indirect illumination
		if (gDoIndirectGI)
		{
			// Select a random direction for our diffuse interreflection ray.
			float3 bounceDir;
			if (gCosSampling)
				bounceDir = getCosHemisphereSample(randSeed, worldNorm.xyz);      // Use cosine sampling
			else
				bounceDir = getUniformHemisphereSample(randSeed, worldNorm.xyz);  // Use uniform random samples

			// Get NdotL for our selected ray direction
			float NdotL = saturate(dot(worldNorm.xyz, bounceDir));

			// Shoot our indirect global illumination ray
			float3 bounceColor = shootIndirectRay(worldPos.xyz, bounceDir, gMinT, randSeed);

			// Probability of selecting this ray ( cos/pi for cosine sampling, 1/2pi for uniform sampling )
			float sampleProb = gCosSampling ? (NdotL / M_PI) : (1.0f / (2.0f * M_PI));

			// Accumulate the color.  For performance, terms could (and should) be cancelled here.
			shadeColor += (NdotL * bounceColor * difMatlColor.rgb / M_PI) / sampleProb;
		}
	}
	
	// Save out our AO color
	gOutput[launchIndex] = float4(shadeColor, 1.0f);
}
