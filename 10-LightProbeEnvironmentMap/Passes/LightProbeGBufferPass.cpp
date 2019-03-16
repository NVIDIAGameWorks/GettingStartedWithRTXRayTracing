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

#include "LightProbeGBufferPass.h"
#include <chrono>

// Some global vars, used to simplify changing shader location & entry points
namespace {
	// What environment map should we load?
	const char* kEnvironmentMap = "MonValley_G_DirtRoad_3k.hdr";

	// Where is our shader located?
	const char* kFileRayTrace = "Tutorial10\\lightProbeGBuffer.rt.hlsl";

	// What are the entry points in that shader for various ray tracing shaders?
	const char* kEntryPointRayGen       = "GBufferRayGen";
	const char* kEntryPointMiss0        = "PrimaryMiss";
	const char* kEntryPrimaryAnyHit     = "PrimaryAnyHit";
	const char* kEntryPrimaryClosestHit = "PrimaryClosestHit";

	// Our camera allows us to jitter.  If using MSAA jitter, here are same positions.  Divide by 8 to give (-0.5..0.5)
	const float kMSAA[8][2] = { { 1,-3 },{ -1,3 },{ 5,1 },{ -3,-5 },{ -5,5 },{ -7,-1 },{ 3,7 },{ 7,-7 } };
};

bool LightProbeGBufferPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash our resource manager, ask for requred textures for this pass
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse",
		                                    "MaterialSpecRough", "MaterialExtraParams" });

	// By default, the environment map is a solid light blue texture.  We can specify a different
	//     texture to load at startup by telling our resource manager where to find it. 
	mpResManager->updateEnvironmentMap(kEnvironmentMap);

	// Set the default scene to load
	mpResManager->setDefaultSceneName("Data/pink_room/pink_room.fscene");

	// Create our wrapper around a ray tracing pass.  Tell it where our shaders are, then compile/link the program
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileRayTrace, kEntryPrimaryClosestHit, kEntryPrimaryAnyHit);
	mpRays->compileRayProgram();
	if (mpScene) mpRays->setScene(mpScene);

	// Set up our random number generator by seeding it with the current time 
	auto currentTime = std::chrono::high_resolution_clock::now();
	auto timeInMillisec = std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime);
	mRng = std::mt19937(uint32_t(timeInMillisec.time_since_epoch().count()));

	// Our GUI needs more space than other passes, so enlarge the GUI window.
	setGuiSize(ivec2(250, 220));
    return true;
}

void LightProbeGBufferPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene and pass it to our ray tracer (if initialized)
	mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	if (mpRays) mpRays->setScene(mpScene);
}

void LightProbeGBufferPass::renderGui(Gui* pGui)
{
	int dirty = 0;

	// Allow user to specify thin lens / pinhole camera parameters
	dirty |= (int)pGui->addCheckBox(mUseThinLens ? "Using thin lens model" : "Using pinhole camera model", mUseThinLens);
	if (mUseThinLens)
	{ 
		pGui->addText("     ");
		dirty |= (int)pGui->addFloatVar("f stop", mFStop, 1.0f, 128.0f, 0.01f, true);
		pGui->addText("     ");
		dirty |= (int)pGui->addFloatVar("f plane", mFocalLength, 0.01f, FLT_MAX, 0.01f, true);
	}

	// Allow user to choose type of camera jitter for anti-aliasing
	dirty |= (int)pGui->addCheckBox(mUseJitter ? "Using camera jitter" : "No camera jitter", mUseJitter);
	if (mUseJitter)
	{
		pGui->addText("     ");
		dirty |= (int)pGui->addCheckBox(mUseRandomJitter ? "Randomized jitter" : "8x MSAA jitter", mUseRandomJitter, true);
	}

	// If any of our UI parameters changed, let the pipeline know we're doing something different next frame
	if (dirty) setRefreshFlag();
}

void LightProbeGBufferPass::execute(RenderContext* pRenderContext)
{
	// Check that we're ready to render
	if (!mpRays || !mpRays->readyToRender()) return;

	// Load our textures, but ask the resource manager to clear them to black before returning them
	Texture::SharedPtr wsPos = mpResManager->getClearedTexture("WorldPosition", vec4(0, 0, 0, 0));
	Texture::SharedPtr wsNorm = mpResManager->getClearedTexture("WorldNormal", vec4(0, 0, 0, 0));
	Texture::SharedPtr matDif = mpResManager->getClearedTexture("MaterialDiffuse", vec4(0, 0, 0, 0));
	Texture::SharedPtr matSpec = mpResManager->getClearedTexture("MaterialSpecRough", vec4(0, 0, 0, 0));
	Texture::SharedPtr matExtra = mpResManager->getClearedTexture("MaterialExtraParams", vec4(0, 0, 0, 0));

	// Compute parameters based on our user-exposed controls
	mLensRadius = mFocalLength / (2.0f * mFStop);

	// Pass our background color down to our miss shader
	auto missVars = mpRays->getMissVars(0);
	missVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);
	missVars["gMatDif"] = matDif;

	// Cycle through all geometry instances, bind our g-buffer outputs to the hit shaders for each instance
	for (auto pVars : mpRays->getHitVars(0))
	{
		pVars["gWsPos"] = wsPos;
		pVars["gWsNorm"] = wsNorm;
		pVars["gMatDif"] = matDif;
		pVars["gMatSpec"] = matSpec;
		pVars["gMatExtra"] = matExtra;
	}

	// Pass our camera parameters to the ray generation shader
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gFrameCount"]  = mFrameCount++;
	rayGenVars["RayGenCB"]["gLensRadius"]  = mUseThinLens ? mLensRadius : 0.0f;
	rayGenVars["RayGenCB"]["gFocalLen"]    = mFocalLength;

	// Compute our jitter, either (0,0) as the center or some computed random/MSAA offset
	float xOff = 0.0f, yOff = 0.0f;
	if (mUseJitter)
	{
		// Determine our offset in the pixel
		xOff = mUseRandomJitter ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][0] * 0.0625f;
		yOff = mUseRandomJitter ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][1] * 0.0625f;
	}

	// Set our shader parameters and the scene camera to use the computed jitter
	rayGenVars["RayGenCB"]["gPixelJitter"] = vec2(xOff + 0.5f, yOff + 0.5f);
	mpScene->getActiveCamera()->setJitter(xOff / float(wsPos->getWidth()), yOff / float(wsPos->getHeight()));

	// Launch our ray tracing
	mpRays->execute( pRenderContext, mpResManager->getScreenSize() );
}
