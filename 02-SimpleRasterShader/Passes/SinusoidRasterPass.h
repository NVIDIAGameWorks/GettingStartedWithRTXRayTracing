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

// Our class definition for the SinusoidRasterPass, which displays a slowly changing sine pattern
//    on the screen using a full-screen raster launch.

#pragma once

// Include our base render pass
#include "../SharedUtils/RenderPass.h"

// Include the wrapper that makes launching full-screen raster work simple.
#include "../SharedUtils/FullscreenLaunch.h"


class SinusoidRasterPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, SinusoidRasterPass>
{
public:
    using SharedPtr = std::shared_ptr<SinusoidRasterPass>;

    static SharedPtr create() { return SharedPtr(new SinusoidRasterPass()); }
    virtual ~SinusoidRasterPass() = default;

protected:
	SinusoidRasterPass() : ::RenderPass("Simple Sinusoid (Raster)", "Raster Sinusoid Options") {}

    // Implementation of RenderPass interface
    bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
    void execute(RenderContext* pRenderContext) override;
    void renderGui(Gui* pGui) override;

	// The base RenderPass class defines a number of methods that we can override to specify
	//    what properties this pass has.  
	bool usesRasterization() override { return true; }  // Note that this pass ueses rasterization.
	bool hasAnimation() override { return false; }      // Removes a GUI control that is confusing for this simple demo

    // Internal pass state
	FullscreenLaunch::SharedPtr   mpSinusoidPass;         ///< Our accumulation shader state
    GraphicsState::SharedPtr      mpGfxState;             ///< Our graphics pipeline state
    uint32_t                      mFrameCount = 0;        ///< A frame counter to let our sinusoid animate
	float                         mScaleValue = 0.1f;     ///< A scale value for our sinusoid
};
