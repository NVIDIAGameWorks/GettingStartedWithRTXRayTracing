// Note: This is a simple render node that accumulates samples over multiple frames.
//     It takes an input buffer, accumulates that buffer with prior frames, and 
//     overwrites that accumulated result back to the input buffer.

#pragma once
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/FullscreenLaunch.h"

class SimpleAccumulationPass : public RenderPass, inherit_shared_from_this<RenderPass, SimpleAccumulationPass>
{
public:
	// For consistency with other classes in Falcor / our tutorial code
    using SharedPtr = std::shared_ptr<SimpleAccumulationPass>;
    using SharedConstPtr = std::shared_ptr<const SimpleAccumulationPass>;

	// Various constructors and destructors
    static SharedPtr create(const std::string &bufferToAccumulate) { return std::make_shared<SimpleAccumulationPass>(bufferToAccumulate); }
	SimpleAccumulationPass(const std::string &bufferToAccumulate);
    virtual ~SimpleAccumulationPass() = default;

protected:

    // Implementation of SimpleRenderPass interface
	bool initialize(RenderContext::SharedPtr pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext::SharedPtr pRenderContext, Scene::SharedPtr pScene) override;
    void execute(RenderContext::SharedPtr pRenderContext) override;
    void renderGui(Gui* pGui) override;
    void resize(uint32_t width, uint32_t height) override;
	void stateRefreshed() override;

	// Override some functions that provide information to the RenderPipeline class
	bool appliesPostprocess() override { return true; }
	bool hasAnimation() override { return false; }

	// A helper utility to determine if the current scene (if any) has had any camera motion
	bool hasCameraMoved();

    // Information about the rendering texture we're accumulating into
	std::string                 mAccumChannel;

	// State for our accumulation shader
	FullscreenLaunch::SharedPtr   mpAccumShader;
	GraphicsState::SharedPtr      mpGfxState;
	Texture::SharedPtr            mpLastFrame;
	Fbo::SharedPtr                mpInternalFbo;

	// We stash a copy of our current scene.  Why?  To detect if changes have occurred.
	Scene::SharedPtr            mpScene;
	mat4                        mpLastCameraMatrix;

	// Is our accumulation enabled?
	bool                        mDoAccumulation = true;

	// How many frames have we accumulated so far?
	uint32_t                    mAccumCount = 0;
};
