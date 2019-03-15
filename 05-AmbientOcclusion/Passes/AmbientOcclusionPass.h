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
#include "../SharedUtils/RayLaunch.h"

/** Ray traced ambient occlusion pass.
*/
class AmbientOcclusionPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, AmbientOcclusionPass>
{
public:
    using SharedPtr = std::shared_ptr<AmbientOcclusionPass>;

    static SharedPtr create() { return SharedPtr(new AmbientOcclusionPass()); }
    virtual ~AmbientOcclusionPass() = default;

protected:
	AmbientOcclusionPass() : ::RenderPass("Ambient Occlusion Rays", "Ambient Occlusion Options") {}

    // Implementation of RenderPass interface
    bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
    void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
    void renderGui(Gui* pGui) override;
    void execute(RenderContext* pRenderContext) override;

	// The RenderPass class defines various methods we can override to specify this pass' properties. 
	bool requiresScene() override { return true; }
	bool usesRayTracing() override { return true; }

    // Rendering state
	RayLaunch::SharedPtr                    mpRays;                 ///< Our wrapper around a DX Raytracing pass
    RtScene::SharedPtr                      mpScene;                ///< Our scene file (passed in from app)  
    
	// Various internal parameters
	float                                   mAORadius = 0.0f;       ///< What radius are we using for AO rays (i.e., maxT when ray tracing)
	uint32_t                                mFrameCount = 0;        ///< Frame count used to help seed our shaders' random number generator
	int32_t                                 mNumRaysPerPixel = 1;   ///< How many ambient occlusion rays should we shot per pixel?
};
