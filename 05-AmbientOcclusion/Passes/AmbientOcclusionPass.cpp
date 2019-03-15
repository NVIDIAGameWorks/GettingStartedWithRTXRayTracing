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

#include "AmbientOcclusionPass.h"

// Some global vars, used to simplify changing shader location & entry points
namespace {
	// Where is our shader located?
	const char* kFileRayTrace = "Tutorial05\\aoTracing.rt.hlsl";

	// What function names are used for the shader entry points for various shaders?
	const char* kEntryPointRayGen   = "AoRayGen";
	const char* kEntryPointMiss0    = "AoMiss";
	const char* kEntryAoAnyHit      = "AoAnyHit";
};

bool AmbientOcclusionPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Keep a copy of our resource manager; request needed buffer resources
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", ResourceManager::kOutputChannel });

	// Set the default scene to load
	mpResManager->setDefaultSceneName("Data/pink_room/pink_room.fscene");

	// Create wrapper around a ray tracing pass.  Specify our ray generation shader and ray-specific shaders 
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader (kFileRayTrace, "", kEntryAoAnyHit);  // No closest hit shader needed, pass in a null shader

	// Now that we've passed all our shaders in, compile.  If we have a scene, let it know.
	mpRays->compileRayProgram();
	if (mpScene) mpRays->setScene(mpScene);
    return true;
}

void AmbientOcclusionPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene.  To use our DXR wrappers, we currently require a Falcor::RtScene 
	//    (as Falcor::Scene currently works only for rasterization).  This is a distinction that will 
	//    go away as DirectX Raytracing becomes available widely.  RtScene is derived from Scene, and 
	//    all scenes loaded in this set of tutorial apps are RtScenes, so we just do a cast here.
    mpScene = std::dynamic_pointer_cast<RtScene>(pScene);

	// Pass our scene to our ray tracer (if initialized)
	if (mpRays) mpRays->setScene(mpScene);
	if (!mpScene) return;

	// Set a default AO radius when we load a new scene.
	mAORadius = glm::max(0.1f, mpScene->getRadius() * 0.05f);
}

void AmbientOcclusionPass::renderGui(Gui* pGui)
{
	// Add a GUI option to allow the user to dynamically change the AO radius
    int dirty = 0;
    dirty |= (int)pGui->addFloatVar("AO radius", mAORadius, 1e-4f, 1e38f, mAORadius * 0.01f);
	dirty |= (int)pGui->addIntVar("Num AO Rays", mNumRaysPerPixel, 1, 64);

    // If changed, let other passes know we changed rendering parameters 
    if (dirty) setRefreshFlag();
}


void AmbientOcclusionPass::execute(RenderContext* pRenderContext)
{
	// Get our output buffer; clear it to black.
	Texture::SharedPtr pDstTex = mpResManager->getClearedTexture(ResourceManager::kOutputChannel, vec4(0.0f, 0.0f, 0.0f, 0.0f));

	// Do we have all the resources we need to render?  If not, return
	if (!pDstTex || !mpRays || !mpRays->readyToRender()) return;

	// Set our ray tracing shader variables for our ray generation shader
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gFrameCount"]  = mFrameCount++;
	rayGenVars["RayGenCB"]["gAORadius"]    = mAORadius;
	rayGenVars["RayGenCB"]["gMinT"]        = mpResManager->getMinTDist();  // From the UI dropdown
	rayGenVars["RayGenCB"]["gNumRays"]     = uint32_t(mNumRaysPerPixel);
	rayGenVars["gPos"]    = mpResManager->getTexture("WorldPosition");
	rayGenVars["gNorm"]   = mpResManager->getTexture("WorldNormal");
	rayGenVars["gOutput"] = pDstTex;

	// Shoot our AO rays
	mpRays->execute( pRenderContext, mpResManager->getScreenSize() );
}



