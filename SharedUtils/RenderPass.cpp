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

#include "RenderPass.h"
#include "Externals/dear_imgui/imgui.h"

using namespace Falcor;

// public

bool ::RenderPass::onInitialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
    assert(!mIsInitialized);
	mIsInitialized = initialize(pRenderContext, pResManager);
    return mIsInitialized;
}

void ::RenderPass::onRenderGui(Gui* pGui)
{
    // Record current UI pos/size
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();

    mGuiPosition.x = (int)std::round(pos.x);
    mGuiPosition.y = (int)std::round(pos.y);
    mGuiSize.x = std::max(32, (int)std::round(size.x));
    mGuiSize.y = std::max(32, (int)std::round(size.y));

    renderGui(pGui);
}

void ::RenderPass::onExecute(RenderContext* pRenderContext)
{ 
	mRefreshFlag = false;     // Did come afterwards, but that prevents discovery of a required refresh while rendering
    execute(pRenderContext);
}

void ::RenderPass::onShutdown()
{
    // Enforce that onShutdown() is only called once, on successfully initialized passes.
    assert(mIsInitialized);
    if (mIsInitialized)
    {
        shutdown();
    }
    mIsInitialized = false;
}
