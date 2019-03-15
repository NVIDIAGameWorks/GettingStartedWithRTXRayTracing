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

#include "ThinLensGBufferPass.h"
#include <chrono>

namespace {
	// Where is our shader located?
	const char* kFileRayTrace = "CommonPasses\\thinLensGBuffer.rt.hlsl";

	// What are the entry points in that shader for various ray tracing shaders?
	const char* kEntryPointRayGen       = "GBufferRayGen";
	const char* kEntryPointMiss0        = "PrimaryMiss";
	const char* kEntryPrimaryAnyHit     = "PrimaryAnyHit";
	const char* kEntryPrimaryClosestHit = "PrimaryClosestHit";

	// If we want to jitter the camera to antialias using traditional a traditional 8x MSAA pattern, 
	//     use these positions (which are in the range [-8.0...8.0], so divide by 16 before use)
	const float kMSAA[8][2] = { { 1,-3 },{ -1,3 },{ 5,1 },{ -3,-5 },{ -5,5 },{ -7,-1 },{ 3,7 },{ 7,-7 } };
};

bool ThinLensGBufferPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;

	// We write to these textures; tell our resource manager that we expect them
	mpResManager->requestTextureResource("WorldPosition");
	mpResManager->requestTextureResource("WorldNormal");
	mpResManager->requestTextureResource("MaterialDiffuse");
	mpResManager->requestTextureResource("MaterialSpecRough");
	mpResManager->requestTextureResource("MaterialExtraParams");

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
	setGuiSize(ivec2(250, 300));

    return true;
}

void ThinLensGBufferPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene and pass it to our ray tracer (if initialized)
	mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	if (mpRays) mpRays->setScene(mpScene);
}

void ThinLensGBufferPass::renderGui(Gui* pGui)
{
	int dirty = 0;

	pGui->addText("When using the thin lens, you can specify");
	pGui->addText("the f-stop and the distance to the focal");
	pGui->addText("plane (units are same as the scene file).");
	pGui->addText("For now, the f-stop is approximate and does");
	pGui->addText("not match your photographic experience, as");
	pGui->addText("our demos do not require scenes with");
	pGui->addText("specified measurement units.");
	pGui->addText("");

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

void ThinLensGBufferPass::execute(RenderContext* pRenderContext)
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
	missVars["MissShaderCB"]["gBgColor"] = mBgColor;
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
	rayGenVars["RayGenCB"]["gUseThinLens"] = mUseThinLens;
	rayGenVars["RayGenCB"]["gFrameCount"]  = mFrameCount++;
	rayGenVars["RayGenCB"]["gLensRadius"]  = mLensRadius;
	rayGenVars["RayGenCB"]["gFocalLen"]    = mFocalLength;

	if (mUseJitter)
	{
		// Determine our offset in the pixel
		float xOff = mUseRandomJitter ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][0] * 0.0625f;
		float yOff = mUseRandomJitter ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][1] * 0.0625f;

		// Set our shader and the scene camera to use the computed jitter
		rayGenVars["RayGenCB"]["gPixelJitter"] = vec2( xOff + 0.5f, yOff + 0.5f );
		mpScene->getActiveCamera()->setJitter(xOff / float(wsPos->getWidth()), yOff / float(wsPos->getHeight()));
	}
	else
	{
		// No jitter, so sent our shader values that mean "use center of pixel"
		rayGenVars["RayGenCB"]["gPixelJitter"] = vec2(0.5f, 0.5f);
		mpScene->getActiveCamera()->setJitter(0,0);
	}

	// Launch our ray tracing
	mpRays->execute( pRenderContext, uvec2(wsPos->getWidth(),wsPos->getHeight()) );
}
