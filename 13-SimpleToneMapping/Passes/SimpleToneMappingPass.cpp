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

#include "SimpleToneMappingPass.h"

SimpleToneMappingPass::SimpleToneMappingPass(const std::string &inBuf, const std::string &outBuf)
	: mInChannel(inBuf), mOutChannel(outBuf), ::RenderPass("Simple Tone Mapping", "Tone Mapping Options")
{
}

bool SimpleToneMappingPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	if (!pResManager) return false;

	// Stash our resource manager; ask for the textures the developer asked for input & output
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ mInChannel, mOutChannel });

	// Set the default scene to load
	mpResManager->setDefaultSceneName("Data/pink_room/pink_room.fscene");

	// We'll use Falcor's built-in tonemapping utility.  Initialize that.
	mpToneMapper = ToneMapping::create(ToneMapping::Operator::Clamp);

	// The Falcor tonemapper messes with DX pipeline state, so create a state object for tonemapping
	mpGfxState = GraphicsState::create();
	return true;
}

void SimpleToneMappingPass::renderGui(Gui* pGui)
{
	// Let the Falcor tonemapper put it's UI in an appropriate spot
	mpToneMapper->renderUI(pGui, nullptr); 
}

void SimpleToneMappingPass::execute(RenderContext* pRenderContext)
{
	if (!mpResManager) return;
   
	// Create framebuffer objects for our input & output textures.  
    Texture::SharedPtr srcTex = mpResManager->getTexture(mInChannel);
	Fbo::SharedPtr dstFbo = mpResManager->createManagedFbo({ mOutChannel });

	// Execute our tone mapping pass.  We do a push/pop rendering state, since
	//     Falcor's tonemapper is known to have side-effects on the current graphics
	//     state...  Thus we want to use a new, disposible render state rather than 
	//     risk weird errors in later passes in out pipeline.
	pRenderContext->pushGraphicsState(mpGfxState);
		mpToneMapper->execute(pRenderContext, srcTex, dstFbo);
	pRenderContext->popGraphicsState();
}


