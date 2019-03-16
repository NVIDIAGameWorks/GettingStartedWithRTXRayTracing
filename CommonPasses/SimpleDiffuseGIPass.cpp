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

#include "SimpleDiffuseGIPass.h"

// Some global vars, used to simplify changing shader location & entry points
namespace {
	// Where is our shaders located?
	const char* kFileRayTrace = "Tutorial12\\simpleDiffuseGI.rt.hlsl";

	// What are the entry points in that shader for various ray tracing shaders?
	const char* kEntryPointRayGen        = "SimpleDiffuseGIRayGen";

	const char* kEntryPointMiss0         = "ShadowMiss";
	const char* kEntryShadowAnyHit       = "ShadowAnyHit";
	const char* kEntryShadowClosestHit   = "ShadowClosestHit";

	const char* kEntryPointMiss1         = "IndirectMiss";
	const char* kEntryIndirectAnyHit     = "IndirectAnyHit";
	const char* kEntryIndirectClosestHit = "IndirectClosestHit";
};

SimpleDiffuseGIPass::SimpleDiffuseGIPass(const std::string &outBuf) 
	: ::RenderPass("Simple Diffuse GI Ray", "Simple Diffuse GI Options"), mOutputBuf(outBuf) 
{
}

bool SimpleDiffuseGIPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse" });
	mpResManager->requestTextureResource( mOutputBuf );
	 
	// We also need our light probe, since indirect rays may hit it
	mpResManager->requestTextureResource(ResourceManager::kEnvironmentMap);

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
	if (mpScene) mpRays->setScene(mpScene);
    return true;
}

void SimpleDiffuseGIPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene and pass it to our ray tracer (if initialized)
    mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	if (mpRays) mpRays->setScene(mpScene);
}

void SimpleDiffuseGIPass::renderGui(Gui* pGui)
{
	// Add a toggle to turn on/off shooting of indirect GI rays
	int dirty = 0;
	dirty |= (int)pGui->addCheckBox(mDoDirectShadows ? "Shooting direct shadow rays" : "No direct shadow rays", mDoDirectShadows);
	dirty |= (int)pGui->addCheckBox(mDoIndirectGI ? "Shooting global illumination rays" : "Skipping global illumination", 
		                            mDoIndirectGI);
	dirty |= (int)pGui->addCheckBox(mDoCosSampling ? "Use cosine sampling" : "Use uniform sampling", mDoCosSampling);
	if (dirty) setRefreshFlag();
}


void SimpleDiffuseGIPass::execute(RenderContext* pRenderContext)
{
	// Get the output buffer we're writing into
	Texture::SharedPtr pDstTex = mpResManager->getClearedTexture(mOutputBuf, vec4(0.0f, 0.0f, 0.0f, 0.0f));

	// Do we have all the resources we need to render?  If not, return
	if (!pDstTex || !mpRays || !mpRays->readyToRender()) return;

	// Set our shader variables for the ray generation shader
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gMinT"]         = mpResManager->getMinTDist();
	rayGenVars["RayGenCB"]["gFrameCount"]   = mFrameCount++;
	rayGenVars["RayGenCB"]["gDoIndirectGI"] = mDoIndirectGI;
	rayGenVars["RayGenCB"]["gCosSampling"]  = mDoCosSampling;
	rayGenVars["RayGenCB"]["gDirectShadow"] = mDoDirectShadows;

	// Pass our G-buffer textures down to the HLSL so we can shade
	rayGenVars["gPos"]         = mpResManager->getTexture("WorldPosition");
	rayGenVars["gNorm"]        = mpResManager->getTexture("WorldNormal");
	rayGenVars["gDiffuseMatl"] = mpResManager->getTexture("MaterialDiffuse");
	rayGenVars["gOutput"]      = pDstTex;

	// Set our environment map texture for indirect rays that miss geometry 
	auto missVars = mpRays->getMissVars(1);       // Remember, indirect rays are ray type #1
	missVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Execute our shading pass and shoot indirect rays
	mpRays->execute( pRenderContext, uvec2(pDstTex->getWidth(), pDstTex->getHeight()) );
}


