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

#include "ConstantColorPass.h"

bool ConstantColorPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	//    We need an output buffer; tell our resource manager we expect the standard output channel
	mpResManager = pResManager;
	mpResManager->requestTextureResource(ResourceManager::kOutputChannel);

    return true;  // Successful initialization.
}

void ConstantColorPass::renderGui(Gui* pGui)
{
	// Add a widget to our GUI to allow us to dynamically change the constant color
	pGui->addFloat3Var("  Color", mConstColor, 0.0f, 1.0f);
}

void ConstantColorPass::execute(RenderContext* pRenderContext)
{
	// Get a pointer to a Falcor texture resource of our output channel
	Texture::SharedPtr outTex = mpResManager->getTexture(ResourceManager::kOutputChannel);

	// Clear the texture to the appropriate color
	mpResManager->clearTexture(outTex, vec4(mConstColor, 1.0f));
}
