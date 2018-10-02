#pragma once
#include "../SharedUtils/SimpleRenderPass.h"
#include "../SharedUtils/RasterPass.h"
#include <random>

class JitteredGBufferPass : public RenderPass, inherit_shared_from_this<RenderPass, JitteredGBufferPass>
{
public:
    using SharedPtr = std::shared_ptr<JitteredGBufferPass>;
    using SharedConstPtr = std::shared_ptr<const JitteredGBufferPass>;

    static SharedPtr create() { return SharedPtr(new JitteredGBufferPass()); }
    virtual ~JitteredGBufferPass() = default;

protected:
	JitteredGBufferPass() : RenderPass("Jittered G-Buffer", "Jittered G-Buffer Options") {}

    // Implementation of RenderPass interface
    bool initialize(RenderContext::SharedPtr pRenderContext, ResourceManager::SharedPtr pResManager) override;
    void execute(RenderContext::SharedPtr pRenderContext) override;
	void renderGui(Gui* pGui) override;
	void initScene(RenderContext::SharedPtr pRenderContext, Scene::SharedPtr pScene) override;

	// Override some functions that provide information to the RenderPipeline class
	bool requiresScene() override { return true; }
	bool usesRasterization() override { return true; }

    // Internal pass state
    GraphicsState::SharedPtr    mpGfxState;             ///< Our graphics pipeline state (i.e., culling, raster, blend settings)
	Scene::SharedPtr            mpScene;                ///< A pointer to the scene we're rendering
	RasterPass::SharedPtr       mpRaster;               ///< A wrapper managing the shader for our g-buffer creation
	bool                        mUseJitter = true;      ///< Jitter the camera?
	bool                        mUseRandom = false;     ///< If jittering, use random samples or 8x MSAA pattern?
	int                         mFrameCount = 0;        ///< If jittering the camera, which frame in our jitter are we on?

	// Our random number generator (if we're doing randomized samples)
	std::uniform_real_distribution<float> mRngDist;     ///< We're going to want random #'s in [0...1] (the default distribution)
	std::mt19937 mRng;                                  ///< Our random number generate.  Set up in initialize()

	// What's our background color?
	vec3  mBgColor = vec3(0.5f, 0.5f, 1.0f);            ///<  Color stored into our diffuse G-buffer channel if we hit no geometry
};
