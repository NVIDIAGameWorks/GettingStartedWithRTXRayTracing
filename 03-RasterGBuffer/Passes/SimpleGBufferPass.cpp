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

#include "SimpleGBufferPass.h"

// Some global vars, used to simplify changing shader locations
namespace {
	// Where are our shaders located?
	const char *kGbufVertShader = "Tutorial03\\gBuffer.vs.hlsl";
    const char *kGbufFragShader = "Tutorial03\\gBuffer.ps.hlsl";
};

bool SimpleGBufferPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;

	// We need a bunch of textures to store our G-buffer.  Ask for a list of them.  They all get the same 
	//     format (in this case, the default, RGBA32F) and size (in this case, the default, screen sized)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse", 
		                                    "MaterialSpecRough", "MaterialExtraParams" });

	// We also need a depth buffer to use when rendering our g-buffer.  Ask for one, with appropriate format and binding flags.
	mpResManager->requestTextureResource("Z-Buffer", ResourceFormat::D24UnormS8, ResourceManager::kDepthBufferFlags);

	// Set the default scene to load
	mpResManager->setDefaultSceneName("Data/pink_room/pink_room.fscene");

    // Since we're rasterizing, we need to define our raster pipeline state (though we use the defaults)
    mpGfxState = GraphicsState::create();

	// Create our wrapper for a scene-rasterization pass.
	mpRaster = RasterLaunch::createFromFiles(kGbufVertShader, kGbufFragShader);
	mpRaster->setScene(mpScene);

    return true;
}

void SimpleGBufferPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene
	if (pScene) 
		mpScene = pScene;

	// Update our raster pass wrapper with this scene
	if (mpRaster) 
		mpRaster->setScene(mpScene);
}

void SimpleGBufferPass::execute(RenderContext* pRenderContext)
{
	// Create a framebuffer for rendering.  (Creating once per frame is for simplicity, not performance).
	Fbo::SharedPtr outputFbo = mpResManager->createManagedFbo(
		{ "WorldPosition", "WorldNormal", "MaterialDiffuse", "MaterialSpecRough", "MaterialExtraParams" }, // Names of color buffers
		"Z-Buffer" );                                                                                      // Name of depth buffer

    // Failed to create a valid FBO?  We're done.
    if (!outputFbo) return;

	// Clear g-buffer.  Clear colors to black, depth to 1, stencil to 0, but then clear diffuse texture to our bg color
	pRenderContext->clearFbo(outputFbo.get(), vec4(0, 0, 0, 0), 1.0f, 0);        
	pRenderContext->clearUAV(outputFbo->getColorTexture(2)->getUAV().get(), vec4(mBgColor, 1.0f));  

	// Execute our rasterization pass.  Note: Falcor will populate many built-in shader variables
	mpRaster->execute(pRenderContext, mpGfxState, outputFbo);
}
