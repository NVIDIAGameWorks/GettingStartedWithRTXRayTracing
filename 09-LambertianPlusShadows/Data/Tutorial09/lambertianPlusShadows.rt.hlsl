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
#include "lambertianPlusShadowsUtils.hlsli"

// Payload for our shadow rays. 
struct ShadowRayPayload
{
	float visFactor;  // Will be 1.0 for fully lit, 0.0 for fully shadowed
};

// A constant buffer we'll populate from our C++ code 
cbuffer RayGenCB
{
	float gMinT;      // Min distance to start a ray to avoid self-occlusion
}

// Input and out textures that need to be set by the C++ code
Texture2D<float4>   gPos;           // G-buffer world-space position
Texture2D<float4>   gNorm;          // G-buffer world-space normal
Texture2D<float4>   gDiffuseMatl;   // G-buffer diffuse material (RGB) and opacity (A)
RWTexture2D<float4> gOutput;        // Output to store shaded result

// Helper function to shoot shadow rays.  In: ray origin, dir, & min/max dist;  Out: 1=lit, 0=shadowed
float shadowRayVisibility( float3 origin, float3 direction, float minT, float maxT )
{
	// Setup our shadow ray
	RayDesc ray;
	ray.Origin = origin;        // Where does it start?
	ray.Direction = direction;  // What direction do we shoot it?
	ray.TMin = minT;            // The closest distance we'll count as a hit
	ray.TMax = maxT;            // The farthest distance we'll count as a hit

	// Our shadow rays are *assumed* to hit geometry; this miss shader changes this to 1.0 for "visible"
	ShadowRayPayload payload = { 0.0f };   

	// Query if anything is between the current point and the light
	TraceRay(gRtScene, 
		     RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 
		     0xFF, 0, hitProgramCount, 0, ray, payload);

	// Return our ray payload (which is 1 for visible, 0 for occluded)
	return payload.visFactor;
}

// How do we shade our g-buffer and generate shadow rays?
[shader("raygeneration")]
void LambertShadowsRayGen()
{
	// Get our pixel's position on the screen
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim   = DispatchRaysDimensions().xy;

	// Load g-buffer data:  world-space position, normal, and diffuse color
	float4 worldPos     = gPos[launchIndex];
	float4 worldNorm    = gNorm[launchIndex];
	float4 difMatlColor = gDiffuseMatl[launchIndex];

	// If we don't hit any geometry, our difuse material contains our background color.
	float3 shadeColor = difMatlColor.rgb;

	// Our camera sees the background if worldPos.w is 0, only do diffuse shading
	if (worldPos.w != 0.0f)
	{
		// We're going to accumulate contributions from multiple lights, so zero out our sum
		shadeColor = float3(0.0, 0.0, 0.0);

		// Iterate over the lights
		for (int lightIndex = 0; lightIndex < gLightsCount; lightIndex++)
		{
			// We need to query our scene to find info about the current light
			float distToLight;      // How far away is it?
			float3 lightIntensity;  // What color is it?
			float3 toLight;         // What direction is it from our current pixel?

			// A helper (from the included .hlsli) to query the Falcor scene to get this data
			getLightData(lightIndex, worldPos.xyz, toLight, lightIntensity, distToLight);

			// Compute our lambertion term (L dot N)
			float LdotN = saturate(dot(worldNorm.xyz, toLight));

			// Shoot our ray.  Return 1.0 for lit, 0.0 for shadowed
			float shadowMult = shadowRayVisibility(worldPos.xyz, toLight, gMinT, distToLight);

			// Accumulate our Lambertian shading color
			shadeColor += shadowMult * LdotN * lightIntensity; 
		}

		// Modulate based on the physically based Lambertian term (albedo/pi)
		shadeColor *= difMatlColor.rgb / 3.141592f;
	}

	// Save out our final shaded
	gOutput[launchIndex] = float4(shadeColor, 1.0f);
}


// What code is executed when our ray misses all geometry?
[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayData)
{
	// If we miss all geometry, then the light is visibile
	rayData.visFactor = 1.0f;
}

// What code is executed when our ray hits a potentially transparent surface?
[shader("anyhit")]
void ShadowAnyHit(inout ShadowRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Is this a transparent part of the surface?  If so, ignore this hit
	if (alphaTestFails(attribs))
		IgnoreHit();
}

// What code is executed when we have a new closest hitpoint?
[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
}