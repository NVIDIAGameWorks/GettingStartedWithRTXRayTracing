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

// This render pass is similar to the "LambertianPlusShadowPass" in Tutorial #9, except in this
//      pass, we *randomly* select just one light to trace a shadow ray towards.  This is especially
//      helpful in scenes with a large number of lights.  It also provides a very simple example
//      of Monte Carlo sampling for shadows.  This adds noise, but it is reduced quickly when
//      applying our temporal accumulation pass from Tutorial #6.  Additionally, the noise from
//      stochastic sampling of direct illumination is much less than the noise we'll introduce
//      when we start stochastically sampling indirect light (in Tutorial #12), so adding a little
//      noise with random shadow rays is not such a crazy idea.


#pragma once
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/RayLaunch.h"

class DiffuseOneShadowRayPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, DiffuseOneShadowRayPass>
{
public:
    using SharedPtr = std::shared_ptr<DiffuseOneShadowRayPass>;
    using SharedConstPtr = std::shared_ptr<const DiffuseOneShadowRayPass>;

    static SharedPtr create() { return SharedPtr(new DiffuseOneShadowRayPass()); }
    virtual ~DiffuseOneShadowRayPass() = default;

protected:
	DiffuseOneShadowRayPass() : ::RenderPass("Diffuse + 1 Rand Shadow Ray", "Diffuse + 1 Random Shadow  Options") {}

    // Implementation of RenderPass interface
    bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
    void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
    void execute(RenderContext* pRenderContext) override;

	// Override some functions that provide information to the RenderPipeline class
	bool requiresScene() override { return true; }
	bool usesRayTracing() override { return true; }

    // Rendering state
	RayLaunch::SharedPtr                    mpRays;                 ///< Our wrapper around a DX Raytracing pass
    RtScene::SharedPtr                      mpScene;                ///< Our scene file (passed in from app)  
    
	// Various internal parameters
	uint32_t                                mFrameCount = 0x1337u;  ///< A frame counter to vary random numbers over time
};
