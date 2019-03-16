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

namespace {
	// Where is our shaders located?
	const char* kFileRayTrace = "CommonPasses\\aoTracing.rt.hlsl";

	// What are the entry points in that shader for various ray tracing shaders?
	const char* kEntryPointRayGen   = "AoRayGen";
	const char* kEntryPointMiss0    = "AoMiss";
	const char* kEntryAoAnyHit      = "AoAnyHit";
	const char* kEntryAoClosestHit  = "AoClosestHit";
};

bool AmbientOcclusionPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;

	// Note that we need the G-buffer's position and normal buffer, plus the standard output buffer
	mPositionIndex = mpResManager->requestTextureResource("WorldPosition");
	mNormalIndex   = mpResManager->requestTextureResource("WorldNormal");
	mOutputIndex   = mpResManager->requestTextureResource(mOutputTexName);

	// Create our wrapper around a ray tracing pass.  Tell it where our ray generation shader and ray-specific shaders are
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileRayTrace, kEntryAoClosestHit, kEntryAoAnyHit);

	// Now that we've passed all our shaders in, compile.  If we already have our scene, let it know what scene to use.
	mpRays->compileRayProgram();
	if (mpScene) mpRays->setScene(mpScene);

    return true;
}

void AmbientOcclusionPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene and pass it to our ray tracer (if initialized)
    mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	if (!mpScene) return;
	if (mpRays) mpRays->setScene(mpScene);
   
	// Set a default AO radius when we load a new scene.
	mAORadius = glm::max(0.1f, mpScene->getRadius() * 0.05f);
}

void AmbientOcclusionPass::renderGui(Gui* pGui)
{
    int dirty = 0;
    dirty |= (int)pGui->addFloatVar("AO radius", mAORadius, 1e-4f, 1e38f, mAORadius * 0.01f);
	dirty |= (int)pGui->addIntVar("Num AO Rays", mNumRaysPerPixel, 1, 64);

    // If we modify options, let our pipeline know that we changed our rendering parameters 
    if (dirty) setRefreshFlag();
}


void AmbientOcclusionPass::execute(RenderContext* pRenderContext)
{
	// Get our output buffer; clear it to black.
	Texture::SharedPtr pDstTex = mpResManager->getClearedTexture(mOutputIndex, vec4(0.0f, 0.0f, 0.0f, 0.0f));

	// Do we have all the resources we need to render?  If not, return
	if (!pDstTex || !mpRays || !mpRays->readyToRender()) return;

	// Set our ray tracing shader variables (just for the ray gen shader here)
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gFrameCount"]  = mFrameCount++;
	rayGenVars["RayGenCB"]["gAORadius"]    = mAORadius;
	rayGenVars["RayGenCB"]["gMinT"]        = mpResManager->getMinTDist();  // From the UI dropdown
	rayGenVars["RayGenCB"]["gNumRays"]     = uint32_t(mNumRaysPerPixel);
	rayGenVars["gPos"]    = mpResManager->getTexture(mPositionIndex);
	rayGenVars["gNorm"]   = mpResManager->getTexture(mNormalIndex);
	rayGenVars["gOutput"] = pDstTex;

	// Shoot our AO rays
	mpRays->execute( pRenderContext, uvec2(pDstTex->getWidth(), pDstTex->getHeight()) );
}



