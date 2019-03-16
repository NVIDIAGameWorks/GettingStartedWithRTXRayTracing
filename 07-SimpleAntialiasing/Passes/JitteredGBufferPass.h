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

// This render pass updates the "SimpleGBufferPass" from Tutorial #3 to include camera jitter.
//      Each frame, a new and slightly offset camera position is selected (either randomly within 
//      the pixel or using a standard 8x MSAA pattern).  This helps break up aliasing along edges,
//      especially when combined with temporal accumulation (since this pass *does not* accumulate).

#pragma once
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/RasterLaunch.h"
#include <random>

class JitteredGBufferPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, JitteredGBufferPass>
{
public:
    using SharedPtr = std::shared_ptr<JitteredGBufferPass>;

    static SharedPtr create() { return SharedPtr(new JitteredGBufferPass()); }
    virtual ~JitteredGBufferPass() = default;

protected:
	JitteredGBufferPass() : ::RenderPass("Jittered G-Buffer", "Jittered G-Buffer Options") {}

    // Implementation of RenderPass interface
    bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
    void execute(RenderContext* pRenderContext) override;
	void renderGui(Gui* pGui) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;

	// The RenderPass class defines various methods we can override to specify this pass' properties. 
	bool requiresScene() override { return true; }
	bool usesRasterization() override { return true; }

    // Internal pass state
    GraphicsState::SharedPtr    mpGfxState;             ///< Our graphics pipeline state (i.e., culling, raster, blend settings)
	Scene::SharedPtr            mpScene;                ///< A pointer to the scene we're rendering
	RasterLaunch::SharedPtr     mpRaster;               ///< A wrapper managing the shader for our g-buffer creation
	bool                        mUseJitter = true;      ///< Jitter the camera?
	bool                        mUseRandom = false;     ///< If jittering, use random samples or 8x MSAA pattern?
	int                         mFrameCount = 0;        ///< If jittering the camera, which frame in our jitter are we on?

	// Our random number generator (if we're doing randomized samples)
	std::uniform_real_distribution<float> mRngDist;     ///< We're going to want random #'s in [0...1] (the default distribution)
	std::mt19937 mRng;                                  ///< Our random number generator.  Set up in initialize()

	// What's our background color?
	vec3  mBgColor = vec3(0.5f, 0.5f, 1.0f);            ///<  Color stored into our diffuse G-buffer channel if we hit no geometry
};
