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

class CopyToOutputPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, CopyToOutputPass>
{
public:
    using SharedPtr = std::shared_ptr<CopyToOutputPass>;
    using SharedConstPtr = std::shared_ptr<const CopyToOutputPass>;

	static SharedPtr create() { return SharedPtr(new CopyToOutputPass()); }
    virtual ~CopyToOutputPass() = default;

protected:
	// Constructor.  The strings represent:
	//     1) The name of the pass that will be in the dropdown pass selector widget(s)
	//     2) The name of the GUI window showing widget controls for this pass
	CopyToOutputPass() : ::RenderPass("Copy-to-Output Pass", "Copy-to-Output Options") {}
	
	// The initialize() callback will be invoked when this class is instantiated and bound to a pipeline
    bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;

	// The renderGui() callback allows you to attach GUI widget into this pass' options window
    void renderGui(Gui* pGui) override;

	// When the pipeline is updated, we will need to update the list of buffers we can copy from
	void pipelineUpdated(ResourceManager::SharedPtr pResManager) override;

	// The execute() callback is invoked during frame render when it is this pass' turn to execute
    void execute(RenderContext* pRenderContext) override;

	// Override some functions that provide information to the RenderPipeline class
	bool appliesPostprocess() override { return true; }

	Gui::DropdownList mDisplayableBuffers;  
	uint32_t          mSelectedBuffer = 0xFFFFFFFFu;
 };
