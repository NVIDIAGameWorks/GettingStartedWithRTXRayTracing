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

// Include utility functions for sampling random numbers
#include "thinLensUtils.hlsli"

// Define pi
#define M_PI  3.14159265358979323846264338327950288

// Payload for our primary rays.  We really don't use this for this g-buffer pass
struct SimpleRayPayload
{
	bool hit;
};

// A constant buffer we'll fill in for our miss shader
cbuffer MissShaderCB
{
	float3  gBgColor;
};

// Shader parameters for our ray gen shader that need to be set by the C++ code
cbuffer RayGenCB
{
	float   gLensRadius;
	float   gFocalLen;
	uint    gFrameCount;
	bool    gUseThinLens;
	float2  gPixelJitter;   // in [0..1]^2.  Should be (0.5,0.5) if no jittering used
}

// Our output textures, where we store our G-buffer results
RWTexture2D<float4> gWsPos;
RWTexture2D<float4> gWsNorm;
RWTexture2D<float4> gMatDif;
RWTexture2D<float4> gMatSpec;
RWTexture2D<float4> gMatExtra;


[shader("miss")]
void PrimaryMiss(inout SimpleRayPayload hitData)
{
	// Store the background color into our diffuse material buffer
	gMatDif[ DispatchRaysIndex().xy ] = float4( gBgColor, 1.0f );
}

[shader("anyhit")]
void PrimaryAnyHit(inout SimpleRayPayload hitData, BuiltInTriangleIntersectionAttributes attribs)
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
}

[shader("closesthit")]
void PrimaryClosestHit(inout SimpleRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	rayData.hit = true;

	// Get some information about the current ray
	uint2  launchIndex     = DispatchRaysIndex().xy;  

	// Run a pair of Falcor helper functions to compute important data at the current hit point
	VertexOut  vsOut       = getVertexAttributes(PrimitiveIndex(), attribs);        // Get geometrical data
	ShadingData shadeData  = prepareShadingData(vsOut, gMaterial, gCamera.posW, 0); // Get shading data

	// Check if we hit the back of a double-sided material, in which case, we flip
	//     normals around here (so we don't need to when shading)
	float NdotV = dot(normalize(shadeData.N.xyz), normalize(gCamera.posW - shadeData.posW));
	if (NdotV <= 0.0f && shadeData.doubleSidedMaterial)
		shadeData.N = -shadeData.N;

	// Save out our G-Buffer values to our textures
	gWsPos[launchIndex]    = float4(shadeData.posW, 1.f);
	gWsNorm[launchIndex]   = float4(shadeData.N, length(shadeData.posW - gCamera.posW));
	gMatDif[launchIndex]   = float4(shadeData.diffuse, shadeData.opacity);
	gMatSpec[launchIndex]  = float4(shadeData.specular, shadeData.linearRoughness);
	gMatExtra[launchIndex] = float4(shadeData.IoR, shadeData.doubleSidedMaterial ? 1.f : 0.f, 0.f, 0.f);
}


[shader("raygeneration")]
void GBufferRayGen()
{
	// Get our pixel's position on the screen
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim   = DispatchRaysDimensions().xy;

	// Convert our ray index into a ray direction in world space
	float2 pixelCenter = (launchIndex + gPixelJitter) / launchDim;
	float2 ndc = float2(2, -2) * pixelCenter + float2(-1, 1);                    
	float3 rayDir = ndc.x * gCamera.cameraU + ndc.y * gCamera.cameraV + gCamera.cameraW;  
	rayDir /= length(gCamera.cameraW);

	// Find the focal point for this pixel.
	float3 focalPoint = gCamera.posW + gFocalLen * rayDir;

	// Initialize a random number generator
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, gFrameCount, 16);

	// Get random numbers (polar coordinates), convert to random cartesian uv on the lens
	float2 rnd = float2(2.0f * M_PI * nextRand(randSeed), gLensRadius * nextRand(randSeed));
	float2 uv  = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

	// Use uv coordinate to compute a random origin on the camera lens
	float3 randomOrig = gCamera.posW + uv.x * normalize(gCamera.cameraU) + uv.y * normalize(gCamera.cameraV);

	// Initialize a ray structure for our ray tracer
	RayDesc ray; 
	ray.Origin    = gUseThinLens ? randomOrig : gCamera.posW;  
	ray.Direction = normalize(gUseThinLens ? focalPoint - randomOrig : rayDir);
	ray.TMin      = 0.0f;              
	ray.TMax      = 1e+38f;            

	// Initialize our ray payload (a per-ray, user-definable structure)
	SimpleRayPayload rayData = { false };

	// Trace our ray
	TraceRay(gRtScene,                        // Acceleration structure
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,  // Ray flags
		0xFF,                                 // Instance inclusion mask
		0,                                    // Ray type
		hitProgramCount,                      // 
		0,                                    // Miss program index
		ray,                                  // Ray to shoot
		rayData);                             // Our ray payload
}
