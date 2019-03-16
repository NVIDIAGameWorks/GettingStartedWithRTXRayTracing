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

class RayTracedGBufferPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, RayTracedGBufferPass>
{
public:
    using SharedPtr = std::shared_ptr<RayTracedGBufferPass>;

    static SharedPtr create() { return SharedPtr(new RayTracedGBufferPass()); }
    virtual ~RayTracedGBufferPass() = default;

protected:
	RayTracedGBufferPass() : ::RenderPass("Ray Traced G-Buffer", "Ray Traced G-Buffer Options") {}

    // Implementation of RenderPass interface
    bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
    void execute(RenderContext* pRenderContext) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;

	// The base RenderPass class defines a number of methods that we can override to 
	//    specify what properties this pass has.  
	bool requiresScene() override { return true; }
	bool usesRayTracing() override { return true; }

	// Internal pass state
	RayLaunch::SharedPtr        mpRays;            ///< Our wrapper around a DX Raytracing pass
	RtScene::SharedPtr          mpScene;           ///<  A copy of our scene

	// What's our background color?
	vec3                        mBgColor = vec3(0.5f, 0.5f, 1.0f);  
};
