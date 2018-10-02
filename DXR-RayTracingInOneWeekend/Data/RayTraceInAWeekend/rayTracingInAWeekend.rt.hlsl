// This is the main entry point for HLSL in the DXR demo of Pete's "Ray Tracing in One Weekend"
//    GPU code starts running in the ray generation entry point RayTracingInAWeekend(), belwo.

////////////////////////////////////////////////////////////////////////////////////////////////////
// Include Falcor specific data and utilities.
//    * This includes "gCamera", which contains pre-populated camera data from our C++ code
//    * This includes "gRtScene", which countains our DirectX ray accelerations structure
//    * This includes "hitProgramCount", which stores how many hit groups our shader contains

__import Raytracing;                 // See "Data/raytracing.slang" in the binary directory
#include "HostDeviceSharedMacros.h"  // Also in "Data/" directory.

////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: The "shared" keyword below tells our Slang HLSL compiler front end that these variables 
//       belong in the shared namespace for all DXR shader stages (i.e., the global root signature), 
//       and exposes them in the C++ code via reflection into that namespace.  In your code (or 
//       other large frameworks), this keyword is likely to have different semantics, and you may 
//       need to specify root signature variables in a different way.
////////////////////////////////////////////////////////////////////////////////////////////////////


// Shader parameters for our ray gen shader that need to be set by the C++ code
shared cbuffer SharedCB
{
	float  gMinT;            // A small epsilon to (help) avoid self-intersections
	uint   gFrameCount;      // A counter that changes each frame to update our random seed
	uint   gMaxDepth;        // The maximum recursion depth for our ray tracing
	float  gPixelMultiplier; // If doing multiple samples per pixel, the multiplier for each sample
	float  gFocalLen;        // Current focal length
	float  gLensRadius;      // Current radius of the camera lens

	// Parameters allowing the user to dynamically toggle material properties
	bool   gShowDiffuseTextures;
	bool   gShowNormalMaps;
	bool   gPerturbRefractions;
}

// The buffer containing our AABB locations
shared Buffer<float3> gAABBData;
shared Buffer<float4> gMatlData;

// Our output textures, where we store our G-buffer results
shared RWTexture2D<float4> gOutTex;
shared Texture2D<float4> gEarthTex;
shared Texture2D<float4> gMoonTex;
shared Texture2D<float4> gNormalMap;

// A bunch of utilties for random numbers: initRand(), nextRand(), getCosHemisphereSample()
#include "randomUtils.hlsli"

// Includes lots of shading utilities (like isDiffuseMaterial() and shadeDiffuseMaterial())
#include "shadingUtils.hlsli"

// Includes the ray-sphere intersection code, i.e., an appropriate [shader("intersection")]
#include "sphereIntersect.hlsli"

// Includes all shaders and payload data to call shootShadowRay()
#include "shadowRay.hlsli"

// Includes the function shootColorRay() and all associated shaders *except* the ColorRayClosestHit(), below.
#include "colorRay.hlsli"

// Our closest hit shader, which gets executed when a color ray hits a surface.
[shader("closesthit")]
void ColorRayClosestHit(inout ColorPayload pay, SphereAttribs attribs)
{
	// Get our material properties for the current sphere we hit
	uint primId     = PrimitiveIndex();
	float4 matlData = gMatlData[primId];

	// Get information about our ray at the current hit point
	float3 rayOrig  = WorldRayOrigin();
	float3 rayDir   = WorldRayDirection();
	float3 hitPt    = rayOrig + RayTCurrent() * rayDir;

	// Compute our sphere normal analytically based on the hit point and the center
	float3 sphNorm  = normalize( hitPt - attribs.sphereCtr );

	// Should we continue tracing color rays?  Or have we recursed too far?
	bool depthExceeded = pay.depth > gMaxDepth;

	// Shade our current hit, depending on its material properties
	if ( isDiffuseMaterial(matlData) )
	{
		pay.color = shadeDiffuseMaterial( matlData, hitPt, rayDir, sphNorm, pay.rngSeed );
	}
	else if ( !depthExceeded && isMetalMaterial(matlData) )  // Don't shade metal if we recursed too deep
	{
		pay.color = shadeMetalMaterial( matlData, hitPt, rayDir, sphNorm, pay.depth, pay.rngSeed );
	}
	else if ( !depthExceeded && isGlassMaterial(matlData) )  // Don't shade glass if we recursed too deep
	{
		pay.color = shadeGlassMaterial( matlData, hitPt, rayDir, sphNorm, pay.depth, pay.rngSeed );
	}
}

// Entry point for launching all our rays
[shader("raygeneration")]
void RayTracingInAWeekend()
{
	// Get our pixel's position on the screen
	uint2 launchIndex = DispatchRaysIndex();
	uint2 launchDim   = DispatchRaysDimensions();

	// Initialize a random number generator specific to this pixel and this frame
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, gFrameCount);

	// Convert our ray index into a ray direction in world space
	float2 pixelOff    = float2(nextRand(randSeed), nextRand(randSeed));  // Random offset in pixel
	float2 pixelCenter = (launchIndex + pixelOff) / launchDim;            // Pixel ID -> [0..1] over screen
	float2 ndc = float2(2, -2) * pixelCenter + float2(-1, 1);             // Convert to [-1..1]
	float3 rayDir = ndc.x * gCamera.cameraU + ndc.y * gCamera.cameraV + gCamera.cameraW;  // Ray to point on near plane

	// Where is the focal point on the near plane for this pixel? 
	rayDir /= length(gCamera.cameraW);
	float3 focalPt = gCamera.posW + gFocalLen * rayDir;  

	// Pick a random point on the camera lens for this pixel's ray
	float2 rngLens = float2(6.2831853f * nextRand(randSeed), gLensRadius*nextRand(randSeed));
	float2 lensPos = float2(cos(rngLens.x) * rngLens.y, sin(rngLens.x) * rngLens.y);

	// Compute the ray from our random camera sample through the pixel's location on the focal plane.
	float3 randOrig = gCamera.posW + lensPos.x * normalize(gCamera.cameraU) + lensPos.y * normalize(gCamera.cameraV);
	float3 randDir = normalize(focalPt - randOrig);

	// Output our color
	float3 rayColor = shootColorRay(randOrig, randDir, 0, randSeed);
	gOutTex[launchIndex] += float4(gPixelMultiplier * rayColor, 1.0f);
}
