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

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc  

// Include utility functions for random numbers & alpha testing
#include "thinLensUtils.hlsli"

// Payload for our primary rays.  This shader doesn't actually use the data, but it is currently
//    required to use a user-defined payload while tracing a ray.  So define a simple one.
struct SimpleRayPayload
{
	bool dummyValue;
};

// Shader parameters for our ray gen shader that need to be set by the C++ code
cbuffer RayGenCB
{
	float   gLensRadius;    // Radius of the thin lens.  Use 0 for pinhole camera.
	float   gFocalLen;      // Distance to the plane where geometry is in focus
	uint    gFrameCount;    // An integer changing every frame to update the random number
	float2  gPixelJitter;   // in [0..1]^2.  Should be (0.5,0.5) if no jittering used
}

// How do we generate the rays that we trace?
[shader("raygeneration")]
void GBufferRayGen()
{
	// Get our pixel's position on the screen
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim   = DispatchRaysDimensions().xy;

	// Convert our ray index into a ray direction in world space.  
	float2 pixelCenter = (launchIndex + gPixelJitter) / launchDim;
	float2 ndc = float2(2, -2) * pixelCenter + float2(-1, 1);                    
	float3 rayDir = ndc.x * gCamera.cameraU + ndc.y * gCamera.cameraV + gCamera.cameraW;

	// Find the focal point for this pixel.
	rayDir /= length(gCamera.cameraW);                     // Make ray have length 1 along the camera's w-axis.
	float3 focalPoint = gCamera.posW + gFocalLen * rayDir; // Select point on ray a distance gFocalLen along the w-axis

	// Initialize a random number generator
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, gFrameCount, 16);

	// Get random numbers (in polar coordinates), convert to random cartesian uv on the lens
	float2 rnd = float2(2.0f * 3.14159265f * nextRand(randSeed), gLensRadius * nextRand(randSeed));
	float2 uv  = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

	// Use uv coordinate to compute a random origin on the camera lens
	float3 randomOrig = gCamera.posW + uv.x * normalize(gCamera.cameraU) + uv.y * normalize(gCamera.cameraV);

	// Initialize a ray structure for our ray tracer
	RayDesc ray; 
	ray.Origin    = randomOrig;                          // Start our ray at the world-space camera position
	ray.Direction = normalize(focalPoint - randomOrig);  // Our ray direction
	ray.TMin      = 0.0f;                                // Start at 0.0; for camera, no danger of self-intersection
	ray.TMax      = 1e+38f;                              // Maximum distance to look for a ray hit

	// Initialize our ray payload (a per-ray, user-definable structure)
	SimpleRayPayload rayData = { false };

	// Trace our ray
	TraceRay(gRtScene,                        // A Falcor built-in containing the raytracing acceleration structure
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,  // Ray flags.  (Here, we will skip hits with back-facing triangles)
		0xFF,                                 // Instance inclusion mask.  0xFF => no instances discarded from this mask
		0,                                    // Hit group to index (i.e., when intersecting, call hit shader #0)
		hitProgramCount,                      // Number of hit groups ('hitProgramCount' is built-in from Falcor with the right number) 
		0,                                    // Miss program index (i.e., when missing, call miss shader #0)
		ray,                                  // Data structure describing the ray to trace
		rayData);                             // Our user-defined ray payload structure to store intermediate results
}


////////////////////////////////////////////////////////////////////////////////////////
// Note:  Below this point is identical to the original Raytraced G Buffer code
////////////////////////////////////////////////////////////////////////////////////////


// The output textures, where we store our G-buffer results.  See bindings in C++ code.
RWTexture2D<float4> gWsPos;
RWTexture2D<float4> gWsNorm;
RWTexture2D<float4> gMatDif;
RWTexture2D<float4> gMatSpec;
RWTexture2D<float4> gMatExtra;

// A constant buffer used in our miss shader, we'll fill data in from C++ code
cbuffer MissShaderCB
{
	float3  gBgColor;
};

// What code is executed when our ray misses all geometry?
[shader("miss")]
void PrimaryMiss(inout SimpleRayPayload hitData)
{
	// Store the background color into our diffuse material buffer
	gMatDif[DispatchRaysIndex().xy] = float4(gBgColor, 1.0f);
}

// What code is executed when our ray hits a potentially transparent surface?
[shader("anyhit")]
void PrimaryAnyHit(inout SimpleRayPayload hitData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Is this a transparent part of the surface?  If so, ignore this hit
	if (alphaTestFails(attribs))
		IgnoreHit();
}

// What code is executed when we have a new closest hitpoint?
[shader("closesthit")]
void PrimaryClosestHit(inout SimpleRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Get some information about the current ray
	uint2  launchIndex = DispatchRaysIndex().xy;

	// Run a pair of Falcor helper functions to compute important data at the current hit point
	VertexOut  vsOut = getVertexAttributes(PrimitiveIndex(), attribs);             // Get geometrical data
	ShadingData shadeData = prepareShadingData(vsOut, gMaterial, gCamera.posW, 0); // Get shading data

	// Save out our G-Buffer values to our textures
	gWsPos[launchIndex] = float4(shadeData.posW, 1.f);
	gWsNorm[launchIndex] = float4(shadeData.N, length(shadeData.posW - gCamera.posW));
	gMatDif[launchIndex] = float4(shadeData.diffuse, shadeData.opacity);
	gMatSpec[launchIndex] = float4(shadeData.specular, shadeData.linearRoughness);
	gMatExtra[launchIndex] = float4(shadeData.IoR, shadeData.doubleSidedMaterial ? 1.f : 0.f, 0.f, 0.f);
}