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

// Include and import common Falcor utilities and data structures
import Raytracing;
import ShaderCommon;
import Shading;                      // Shading functions, etc     

// A separate file with some simple utility functions: getPerpendicularVector(), initRand(), nextRand()
#include "aoCommonUtils.hlsli"

// Payload for our primary rays.  We really don't use this for this g-buffer pass
struct AORayPayload
{
	float hitDist;
};

// A constant buffer we'll fill in for our ray generation shader
cbuffer RayGenCB
{
	float gAORadius;
	uint  gFrameCount;
	float gMinT;
	uint  gNumRays;
}

// Input and out textures that need to be set by the C++ code
Texture2D<float4> gPos;
Texture2D<float4> gNorm;
RWTexture2D<float4> gOutput;


[shader("miss")]
void AoMiss(inout AORayPayload hitData : SV_RayPayload)
{
}

[shader("anyhit")]
void AoAnyHit(inout AORayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Is this a transparent part of the surface?  If so, ignore this hit
	if (alphaTestFails(attribs))
		IgnoreHit();

	// We update the hit distance with our current hitpoint
	rayData.hitDist = RayTCurrent();
}

[shader("closesthit")]
void AoClosestHit(inout AORayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	rayData.hitDist = RayTCurrent();
}


[shader("raygeneration")]
void AoRayGen()
{
	// Where is this ray on screen?
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim   = DispatchRaysDimensions().xy;

	// Initialize random seed per sample based on a screen position and temporally varying count
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, gFrameCount, 16);

	// Load the position and normal from our g-buffer
	float4 worldPos = gPos[launchIndex];
	float4 worldNorm = gNorm[launchIndex];

	// Default ambient occlusion
	float ambientOcclusion = float(gNumRays);

	// Our camera sees the background if worldPos.w is 0, only shoot an AO ray elsewhere
	if (worldPos.w != 0.0f)  
	{
		// Start accumulating from zero if we don't hit the background
		ambientOcclusion = 0.0f;

		for (int i = 0; i < gNumRays; i++)
		{
			// Sample cosine-weighted hemisphere around the surface normal
			float3 worldDir = getCosHemisphereSample(randSeed, worldNorm.xyz);

			// Setup ambient occlusion ray
			RayDesc rayAO;
			rayAO.Origin = worldPos.xyz;
			rayAO.Direction = worldDir;
			rayAO.TMin = gMinT;
			rayAO.TMax = gAORadius;

			// Initialize the maximum hitT (which will be the return value if we hit no surfaces)
			AORayPayload rayPayload = { gAORadius + 1.0f };

			// Trace our ray
			TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, hitProgramCount, 0, rayAO, rayPayload);

			// If our hit is what we initialized it to, above, we hit no geometry (else we did hit a surface)
			ambientOcclusion += (rayPayload.hitDist > gAORadius) ? 1.0f : 0.0f;
		}
	}
	
	// Save out our AO color
	float aoColor = ambientOcclusion / float(gNumRays);
	gOutput[launchIndex] = float4(aoColor, aoColor, aoColor, 1.0f);
}
