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

#include "HostDeviceSharedMacros.h"
#include "HostDeviceData.h"            // Some #defines used for shading across Falcor

// Include and import common Falcor utilities and data structures
import Raytracing;
import ShaderCommon; 
import Shading;                      // Shading functions, etc   
import Lights;                       // Light structures for our current scene

// A separate file with some simple utility functions: getPerpendicularVector(), initRand(), nextRand()
#include "lambertianPlusShadowsUtils.hlsli"

// Payload for our primary rays.  We really don't use this for this g-buffer pass
struct ShadowRayPayload
{
	int hitDist;
};

// A constant buffer we'll fill in for our ray generation shader
cbuffer RayGenCB
{
	float gMinT;
}

// Input and out textures that need to be set by the C++ code
Texture2D<float4> gPos;
Texture2D<float4> gNorm;
Texture2D<float4> gDiffuseMatl;
Texture2D<float4> gSpecMatl;
RWTexture2D<float4> gOutput;


float shadowRayVisibility( float3 origin, float3 direction, float minT, float maxT )
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

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayData)
{
}

[shader("anyhit")]
void ShadowAnyHit(inout ShadowRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Run a Falcor helper to extract the hit point's geometric data
	VertexOut  vsOut = getVertexAttributes(PrimitiveIndex(), attribs);

    // Extracts the diffuse color from the material (the alpha component is opacity)
    ExplicitLodTextureSampler lodSampler = { 0 };  // Specify the tex lod/mip to use here
    float4 baseColor = sampleTexture(gMaterial.resources.baseColor, gMaterial.resources.samplerState,
        vsOut.texC, gMaterial.baseColor, EXTRACT_DIFFUSE_TYPE(gMaterial.flags), lodSampler);

	// Test if this hit point passes a standard alpha test.  If not, discard/ignore the hit.
	if (baseColor.a < gMaterial.alphaThreshold)
		IgnoreHit();

	// We update the hit distance with our current hitpoint
	rayData.hitDist = RayTCurrent();
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	rayData.hitDist = RayTCurrent();
}


[shader("raygeneration")]
void LambertShadowsRayGen()
{
	// Where is this ray on screen?
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim   = DispatchRaysDimensions().xy;

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

	// Our camera sees the background if worldPos.w is 0, only shoot an AO ray elsewhere
	if (worldPos.w != 0.0f)
	{
		// We're going to accumulate contributions from multiple lights, so zero our our sum
		shadeColor = float3(0.0, 0.0, 0.0);

		for (int lightIndex = 0; lightIndex < gLightsCount; lightIndex++)
		{
			float distToLight;
			float3 lightIntensity;
			float3 toLight;
			// A helper (that queries the Falcor scene to get needed data about this light)
			getLightData(lightIndex, worldPos.xyz, toLight, lightIntensity, distToLight);

			// Compute our lambertion term (L dot N)
			float LdotN = saturate(dot(worldNorm.xyz, toLight));

			// Shoot our ray
			float shadowMult = shadowRayVisibility(worldPos.xyz, toLight, gMinT, distToLight);

			// Compute our Lambertian shading color
			shadeColor += shadowMult * LdotN * lightIntensity; 
		}

		// Physically based Lambertian term is albedo/pi
		shadeColor *= difMatlColor.rgb / 3.141592f;
	}
	
	// Save out our AO color
	gOutput[launchIndex] = float4(shadeColor, 1.0f);
}
