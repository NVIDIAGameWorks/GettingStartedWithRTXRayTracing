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

#include "GGXGlobalIllumination.h"

// Some global vars, used to simplify changing shader location & entry points
namespace {
	// Where is our shaders located?
	const char* kFileRayTrace = "Tutorial14\\ggxGlobalIllumination.rt.hlsl";

	// What are the entry points in that shader for various ray tracing shaders?
	const char* kEntryPointRayGen        = "SimpleDiffuseGIRayGen";

	const char* kEntryPointMiss0         = "ShadowMiss";
	const char* kEntryShadowAnyHit       = "ShadowAnyHit";
	const char* kEntryShadowClosestHit   = "ShadowClosestHit";

	const char* kEntryPointMiss1         = "IndirectMiss";
	const char* kEntryIndirectAnyHit     = "IndirectAnyHit";
	const char* kEntryIndirectClosestHit = "IndirectClosestHit";
};

bool GGXGlobalIlluminationPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse", "MaterialSpecRough", "MaterialExtraParams", "Emissive" });
	mpResManager->requestTextureResource(mOutputTextureName);
	mpResManager->requestTextureResource(ResourceManager::kEnvironmentMap);

	// Set the default scene to load
	mpResManager->setDefaultSceneName("Data/pink_room/pink_room.fscene");

	// Create our wrapper around a ray tracing pass.  Tell it where our ray generation shader and ray-specific shaders are
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);

	// Add ray type #0 (shadow rays)
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileRayTrace, kEntryShadowClosestHit, kEntryShadowAnyHit);

	// Add ray type #1 (indirect GI rays)
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss1);
	mpRays->addHitShader(kFileRayTrace, kEntryIndirectClosestHit, kEntryIndirectAnyHit);

	// Now that we've passed all our shaders in, compile and (if available) setup the scene
	mpRays->compileRayProgram();
	mpRays->setMaxRecursionDepth(uint32_t(mMaxPossibleRayDepth));
	if (mpScene) mpRays->setScene(mpScene);
    return true;
}

void GGXGlobalIlluminationPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene and pass it to our ray tracer (if initialized)
    mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	if (mpRays) mpRays->setScene(mpScene);
}

void GGXGlobalIlluminationPass::renderGui(Gui* pGui)
{
	int dirty = 0;
	dirty |= (int)pGui->addIntVar("Max RayDepth", mUserSpecifiedRayDepth, 0, mMaxPossibleRayDepth);
	dirty |= (int)pGui->addCheckBox(mDoDirectGI ? "Compute direct illumination" : "Skipping direct illumination",
		                            mDoDirectGI);
	dirty |= (int)pGui->addCheckBox(mDoIndirectGI ? "Shooting global illumination rays" : "Skipping global illumination", 
		                            mDoIndirectGI);
	if (dirty) setRefreshFlag();
}


void GGXGlobalIlluminationPass::execute(RenderContext* pRenderContext)
{
	// Get the output buffer we're writing into
	Texture::SharedPtr pDstTex = mpResManager->getClearedTexture(mOutputTextureName, vec4(0.0f, 0.0f, 0.0f, 0.0f));

	// Do we have all the resources we need to render?  If not, return
	if (!pDstTex || !mpRays || !mpRays->readyToRender()) return;

	// Set our variables into the global HLSL namespace
	auto globalVars = mpRays->getGlobalVars();
	globalVars["GlobalCB"]["gMinT"]         = mpResManager->getMinTDist();
	globalVars["GlobalCB"]["gFrameCount"]   = mFrameCount++;
	globalVars["GlobalCB"]["gDoIndirectGI"] = mDoIndirectGI;
	globalVars["GlobalCB"]["gDoDirectGI"]   = mDoDirectGI;
	globalVars["GlobalCB"]["gMaxDepth"]     = mUserSpecifiedRayDepth;
    globalVars["GlobalCB"]["gEmitMult"]     = 1.0f;
	globalVars["gPos"]         = mpResManager->getTexture("WorldPosition");
	globalVars["gNorm"]        = mpResManager->getTexture("WorldNormal");
	globalVars["gDiffuseMatl"] = mpResManager->getTexture("MaterialDiffuse");
	globalVars["gSpecMatl"]    = mpResManager->getTexture("MaterialSpecRough");
	globalVars["gExtraMatl"]   = mpResManager->getTexture("MaterialExtraParams");
    globalVars["gEmissive"]    = mpResManager->getTexture("Emissive");
	globalVars["gOutput"]      = pDstTex;
	globalVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Shoot our rays and shade our primary hit points
	mpRays->execute( pRenderContext, mpResManager->getScreenSize() );

}


