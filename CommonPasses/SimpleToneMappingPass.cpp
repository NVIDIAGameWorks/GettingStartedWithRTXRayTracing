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

	// Stash our resource manager; ask for the texture the developer asked us to accumulate
	mpResManager = pResManager;
	mpResManager->requestTextureResource(mInChannel);
	mpResManager->requestTextureResource(mOutChannel);

	// The Falcor tonemapper can screw with DX pipeline state, so we'll want to create a disposible state object.
	mpGfxState = GraphicsState::create();

	// Falcor has a built-in utility for tonemapping.  Initialize that
	mpToneMapper = ToneMapping::create(ToneMapping::Operator::Clamp);

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
   
	// Probably should do this once outside the main render loop.
    Texture::SharedPtr srcTex = mpResManager->getTexture( mInChannel );
	Fbo::SharedPtr dstFbo     = mpResManager->createManagedFbo({ mOutChannel });

	// Execute our tone mapping pass
	pRenderContext->pushGraphicsState(mpGfxState);
		mpToneMapper->execute(pRenderContext, srcTex, dstFbo);
	pRenderContext->popGraphicsState();
}


