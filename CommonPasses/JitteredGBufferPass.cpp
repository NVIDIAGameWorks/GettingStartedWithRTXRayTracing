#include "JitteredGBufferPass.h"
#include <chrono>

namespace {
	// For basic jittering, we don't need to change our rasterized g-buffer, just jitter the camera position
	const char *kGbufVertShader = "CommonPasses\\gBuffer.vs.hlsl";
    const char *kGbufFragShader = "CommonPasses\\gBuffer.ps.hlsl";

	// If we want to jitter the camera to antialias using traditional a traditional 8x MSAA pattern, 
	//     use these positions (which are in the range [-8.0...8.0], so divide by 16 before use)
	const float kMSAA[8][2] = { {1,-3}, {-1,3}, {5,1}, {-3,-5}, {-5,5}, {-7,-1}, {3,7}, {7,-7} };
};

bool JitteredGBufferPass::initialize(RenderContext::SharedPtr pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;

	// We write to these output textures; tell our resource manager that we expect them to exist
	mpResManager->requestTextureResource("WorldPosition");
	mpResManager->requestTextureResource("WorldNormal");
	mpResManager->requestTextureResource("MaterialDiffuse");
	mpResManager->requestTextureResource("MaterialSpecRough");
	mpResManager->requestTextureResource("MaterialExtraParams");
	mpResManager->requestTextureResource("Z-Buffer", ResourceFormat::D24UnormS8, ResourceManager::kDepthBufferFlags);

    // Create our rasterization state and our raster shader wrapper
    mpGfxState = GraphicsState::create();
	mpRaster   = RasterPass::createFromFiles(kGbufVertShader, kGbufFragShader);
	mpRaster->setScene(mpScene);

	// Set up our random number generator by seeding it with the current time 
	auto currentTime    = std::chrono::high_resolution_clock::now();
	auto timeInMillisec = std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime);
	mRng                = std::mt19937( uint32_t(timeInMillisec.time_since_epoch().count()) );
	
    return true;
}

void JitteredGBufferPass::initScene(RenderContext::SharedPtr pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene.  Update our raster pass wrapper with the scene
	if (pScene) mpScene = pScene;
	if (mpRaster) mpRaster->setScene(mpScene);
}

void JitteredGBufferPass::renderGui(Gui* pGui)
{
	int dirty = 0;

	// Determine whether we're jittering at all
	dirty |= (int)pGui->addCheckBox(mUseJitter ? "Camera jitter enabled" : "Camera jitter disabled", mUseJitter);

	// Select what kind of jitter to use.  Right now, the choices are: 8x MSAA or completely random
	if (mUseJitter)
	{
		dirty |= (int)pGui->addCheckBox(mUseRandom ? "Using randomized camera position" : "Using 8x MSAA pattern", mUseRandom);
	}

	// If UI parameters change, let the pipeline know we're doing something different next frame
	if (dirty) setRefreshFlag();
}

void JitteredGBufferPass::execute(RenderContext::SharedPtr pRenderContext)
{
	// Create a framebuffer for rendering.  (Creating once per frame is for simplicity, not performance).
	Fbo::SharedPtr outputFbo = mpResManager->createManagedFbo(
		{ "WorldPosition", "WorldNormal", "MaterialDiffuse", "MaterialSpecRough", "MaterialExtraParams" }, 
		"Z-Buffer");                                                                                      

	// Failed to create a valid FBO?  We're done.
	if (!outputFbo) return;
	
	// Are we jittering?  If so, update our camera with the current jitter
	if (mUseJitter && mpScene && mpScene->getActiveCamera())
	{
		// Increase our frame count
		mFrameCount++;

		// Determine our offset in the pixel in the range [-0.5...0.5]
		float xOff = mUseRandom ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][0]*0.0625f;
		float yOff = mUseRandom ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][1]*0.0625f;

		// Give our jitter to the scene camera
		mpScene->getActiveCamera()->setJitter( xOff / float(outputFbo->getWidth()), yOff / float(outputFbo->getHeight()));
	}

	// Clear our g-buffer.  All color buffers to (0,0,0,0), depth to 1, stencil to 0
	pRenderContext->clearFbo(outputFbo.get(), vec4(0, 0, 0, 0), 1.0f, 0);

	// Separately clear our diffuse color buffer to the background color, rather than black
	pRenderContext->clearUAV(outputFbo->getColorTexture(2)->getUAV().get(), vec4(mBgColor, 1.0f));

	// Execute our rasterization pass.  Note: Falcor will populate many built-in shader variables
	mpRaster->execute(pRenderContext, mpGfxState, outputFbo);
}
