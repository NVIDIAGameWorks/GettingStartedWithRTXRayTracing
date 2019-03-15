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

#include "CopyToOutputPass.h"

bool CopyToOutputPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	if (!pResManager) return false;

	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;

	// We write to the output texture; tell our resource manager that we expect this channel
	mpResManager->requestTextureResource(ResourceManager::kOutputChannel);

	// When we start, let's assume there are no valid buffers to copy from; create a GUI list that says so
	mDisplayableBuffers.push_back({ -1, "< None >" });

	// We have now finished initializing our pass
    return true;
}

void CopyToOutputPass::renderGui(Gui* pGui)
{
	// Add a widget to allow us to select our buffer to display
	pGui->addDropdown("Displayed", mDisplayableBuffers, mSelectedBuffer);
}

void CopyToOutputPass::execute(RenderContext* pRenderContext)
{
	// Get a pointer to a Falcor texture resource for our output 
	Texture::SharedPtr outTex = mpResManager->getTexture(ResourceManager::kOutputChannel);

	// Our output texture doesn't exist?  We can't do much.
	if (!outTex) return;

	// Grab our input buffer, as selected by the user from the GUI
	Texture::SharedPtr inTex = mpResManager->getTexture( mSelectedBuffer );

	// If we have selected an invalid texture, clear our output to black and return.
	if (!inTex || mSelectedBuffer == uint32_t(-1))
	{
		pRenderContext->clearRtv(outTex->getRTV().get(), vec4(0.0f,0.0f,0.0f, 1.0f));
		return;
	}
	
	// Copy the selected input buffer to our output buffer.
	pRenderContext->blit( inTex->getSRV(), outTex->getRTV() );
}


void CopyToOutputPass::pipelineUpdated(ResourceManager::SharedPtr pResManager)
{
	// This method gets called when the pipeline changes.  We ask the resource manager what resources
	//     are available to use.  We then create a list of these resources to provide to the user 
	//     via the GUI window, so they can choose which to display on screen.

	// This only works if we have a valid resource manager
	if (!pResManager) return;
	mpResManager = pResManager;

	// Clear the GUI's list of displayable textures
	mDisplayableBuffers.clear();

	// We're not allowing the user to display the output buffer, so identify that resource
	int32_t outputChannel = mpResManager->getTextureIndex(ResourceManager::kOutputChannel);

	// Loop over all resources available in the resource manager
	for (uint32_t i = 0; i < mpResManager->getTextureCount(); i++)
	{
		// If this one is the output resource, skip it
		if (i == outputChannel) continue;

		// Add the name of this resource to our GUI's list of displayable resources
		mDisplayableBuffers.push_back({ int32_t(i), mpResManager->getTextureName(i) });

		// If our UI currently had an invalid buffer selected, select this valid one now.
		if (mSelectedBuffer == uint32_t(-1)) mSelectedBuffer = i;
	}

	// If there are no valid textures to select, add a "<None>" entry to our list and select it.
	if (mDisplayableBuffers.size() <= 0)
	{
		mDisplayableBuffers.push_back({ -1, "< None >" });
		mSelectedBuffer = uint32_t(-1);
	}
}