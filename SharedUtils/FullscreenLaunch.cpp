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

#include "FullscreenLaunch.h"

using namespace Falcor;

FullscreenLaunch::SharedPtr FullscreenLaunch::FullscreenLaunch::create(const char *fragShader)
{
	return SharedPtr(new FullscreenLaunch(fragShader));
}

FullscreenLaunch::FullscreenLaunch(const char *fragShader)
{ 
	mpPass = FullScreenPass::create(fragShader);
	mpVars = GraphicsVars::create( mpPass->getProgram()->getActiveVersion()->getReflector() );
	mInvalidVarReflector = true;
}

void FullscreenLaunch::execute(RenderContext::SharedPtr pRenderContext, GraphicsState::SharedPtr pGfxState)
{
    this->execute(pRenderContext.get(), pGfxState);
}

void FullscreenLaunch::execute(RenderContext* pRenderContext, Falcor::GraphicsState::SharedPtr pGfxState)
{
	// Ok.  We're executing.  If we still have an invalid shader variable reflector, we'd better get one now!
	if (mInvalidVarReflector) createGraphicsVariables();

	if (mpPass && mpVars && pGfxState && pRenderContext)
	{
		pRenderContext->pushGraphicsState(pGfxState);
		pRenderContext->pushGraphicsVars(mpVars);
			mpPass->execute(pRenderContext);
		pRenderContext->popGraphicsVars();
		pRenderContext->popGraphicsState();
	}
}

void FullscreenLaunch::createGraphicsVariables()
{
	// Do we need to recreate our variables?  Do we also have a valid shader?
	if (mInvalidVarReflector && mpPass && mpPass->getProgram())
	{
		mpVars       = GraphicsVars::create(mpPass->getProgram()->getActiveVersion()->getReflector());
		mpSimpleVars = SimpleVars::create(mpVars.get());
		mInvalidVarReflector = false;
	}
}

SimpleVars::SharedPtr FullscreenLaunch::getVars()
{
	if (mInvalidVarReflector)
		createGraphicsVariables();

	return mpSimpleVars;
}

void FullscreenLaunch::addDefine(const std::string& name, const std::string& value)
{
	mpPass->getProgram()->addDefine(name, value);
	mInvalidVarReflector = true;
}

void FullscreenLaunch::removeDefine(const std::string& name)
{
	mpPass->getProgram()->removeDefine(name);
	mInvalidVarReflector = true;
}


void FullscreenLaunch::setCamera(Falcor::Camera::SharedPtr pActiveCamera)
{
	// Shouldn't need to change unless Falcor internals do
	const char*__internalCB = "InternalPerFrameCB";
	const char*__internalVarName = "gCamera";

	// Actually set the internals
	ConstantBuffer::SharedPtr perFrameCB = mpVars[__internalCB];
	if (perFrameCB)
	{
		pActiveCamera->setIntoConstantBuffer(perFrameCB.get(), __internalVarName);
	}
}

void FullscreenLaunch::setLights(const std::vector< Falcor::Light::SharedPtr > &pLights)
{
	// Shouldn't need to change unless Falcor internals do
	const char*__internalCB = "InternalPerFrameCB";
	const char*__internalCountName = "gLightsCount";
	const char*__internalLightsName = "gLights";

	// Actually set the internals
	ConstantBuffer::SharedPtr perFrameCB = mpVars[__internalCB];
	if (perFrameCB)
	{
		perFrameCB[__internalCountName] = uint32_t(pLights.size());
		const auto& pLightOffset = perFrameCB->getBufferReflector()->findMember(__internalLightsName);
		size_t lightOffset = pLightOffset ? pLightOffset->getOffset() : ConstantBuffer::kInvalidOffset;
		for (uint32_t i = 0; i < uint32_t(pLights.size()); i++)
		{
			pLights[i]->setIntoProgramVars(mpVars.get(), perFrameCB.get(), i * Light::getShaderStructSize() + lightOffset);
		}
	}
}

