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

class SimpleToneMappingPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, SimpleToneMappingPass>
{
public:
    using SharedPtr = std::shared_ptr<SimpleToneMappingPass>;
    using SharedConstPtr = std::shared_ptr<const SimpleToneMappingPass>;

    static SharedPtr create(const std::string &inBuf, const std::string &outBuf) { return SharedPtr(new SimpleToneMappingPass(inBuf, outBuf)); }
    virtual ~SimpleToneMappingPass() = default;

protected:
	SimpleToneMappingPass(const std::string &inBuf, const std::string &outBuf);

    // Implementation of SimpleRenderPass interface
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
    void execute(RenderContext* pRenderContext) override;
    void renderGui(Gui* pGui) override;

	// Override some functions that provide information to the RenderPipeline class
	bool appliesPostprocess() override { return true; }

      
	GraphicsState::SharedPtr    mpGfxState;
	std::string                 mInChannel;         ///< What resource are we expecting as our input?
	std::string                 mOutChannel;        ///< What resource should we dump our output into?
	ToneMapping::SharedPtr      mpToneMapper;       ///< Falcor's utility class for tone-mapping.
};
