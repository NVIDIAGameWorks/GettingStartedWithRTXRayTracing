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

#pragma once
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/SimpleVars.h"
#include "../SharedUtils/RayLaunch.h"

/** Ray traced ambient occlusion pass.
*/
class LambertianPlusShadowPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, LambertianPlusShadowPass>
{
public:
    using SharedPtr = std::shared_ptr<LambertianPlusShadowPass>;
    using SharedConstPtr = std::shared_ptr<const LambertianPlusShadowPass>;

    static SharedPtr create() { return SharedPtr(new LambertianPlusShadowPass()); }
    virtual ~LambertianPlusShadowPass() = default;

protected:
	LambertianPlusShadowPass() : ::RenderPass("Lambertian Plus Shadows", "Lambertian Plus Shadow Options") {}

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
	uint32_t                                mMinTSelector = 1;      ///< Allow user to select which minT value to use for rays
};
