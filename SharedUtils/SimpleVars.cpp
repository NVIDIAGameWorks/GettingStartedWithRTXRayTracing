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

#include "SimpleVars.h"

using namespace Falcor;

SimpleVars::SharedPtr SimpleVars::SimpleVars::create(Falcor::Program::SharedPtr pProg)
{
	return SharedPtr(new SimpleVars( GraphicsVars::create(pProg->getActiveVersion()->getReflector()).get() ));
}

SimpleVars::SharedPtr SimpleVars::SimpleVars::create(Falcor::GraphicsVars *pVars)
{
	return SharedPtr(new SimpleVars( pVars ));
}

SimpleVars::SimpleVars(Falcor::GraphicsVars *pVars)
{
	mpVars = pVars;
}

#if 0
void SimpleVars::updateActiveShader()
{
	mpVars = GraphicsVars::create(mpPass->getProgram()->getActiveVersion()->getReflector());
}
#endif

bool SimpleVars::setTexture(const std::string& name, const Texture::SharedPtr& pTexture)
{
	// Ensure a variable with this name exists and is actually a texture!
	if (!isVarValid(name, ReflectionResourceType::Type::Texture)) return false;

	// Go ahead and set it.
	return mpVars->setTexture(name, pTexture);
}

bool SimpleVars::setSampler(const std::string& name, const Falcor::Sampler::SharedPtr& pSampler)
{
	// Ensure a variable with this name exists and is actually a sampler!
	if (!isVarValid(name, ReflectionResourceType::Type::Sampler)) return false;

	// Go ahead and set it.
	return mpVars->setSampler(name, pSampler);
}

bool SimpleVars::setTypedBuffer(const std::string& name, Falcor::TypedBufferBase::SharedPtr& pBuffer)
{
	// Ensure a variable with this name exists and is actually a typed buffer!
	if (!isVarValid(name, ReflectionResourceType::Type::TypedBuffer)) return false;

	// Go ahead and set it.
	return mpVars->setTypedBuffer(name, pBuffer);
}

bool SimpleVars::setStructuredBuffer(const std::string& name, Falcor::StructuredBuffer::SharedPtr& pBuffer)
{
	// Ensure a variable with this name exists and is actually a structured buffer!
	if (!isVarValid(name, ReflectionResourceType::Type::StructuredBuffer)) return false;

	// Go ahead and set it.
	return mpVars->setStructuredBuffer(name, pBuffer);
}

bool SimpleVars::setRawBuffer(const std::string& name, Falcor::Buffer::SharedPtr& pBuffer)
{
	// Ensure a variable with this name exists and is actually a buffer!
	if (!isVarValid(name, ReflectionResourceType::Type::RawBuffer)) return false;

	// Go ahead and set it.
	return mpVars->setRawBuffer(name, pBuffer);
}

bool SimpleVars::isVarValid(const std::string &varName, ReflectionResourceType::Type varType)
{
	ReflectionVar::SharedConstPtr mRes = mpVars->getReflection()->getResource(varName);
	if (mRes.get())
	{
		const ReflectionResourceType* pType = mRes->getType()->unwrapArray()->asResourceType();
		if (pType && pType->getType() == varType)
		{
			return true;
		}
	}
	return false;
}

void SimpleVars::SharedPtr::Idx1::operator=(const Falcor::Texture::SharedPtr& pTexture)
{ 
	// Set the texture
	bool wasSet = mpBuf->setTexture(mVar, pTexture);

	// If you triggered this assert, your call 'myFSPass["someName"] = myTexture' failed for various reasons.
	// You can comment it out if you don't mind that failed assignments will fail silently, without changing any state.
	assert(wasSet);
}

void SimpleVars::SharedPtr::Idx1::operator=(const Falcor::Sampler::SharedPtr& pSampler)
{
	// Set the sampler
	bool wasSet = mpBuf->setSampler(mVar, pSampler);

	// If you triggered this assert, your call 'myFSPass["someName"] = mySampler' failed for various reasons.
	// You can comment it out if you don't mind that failed assignments will fail silently, without changing any state.
	assert(wasSet);
}

void SimpleVars::SharedPtr::Idx1::operator=(Falcor::Buffer::SharedPtr& pBuffer)
{
	// Set the buffer
	bool wasSet = mpBuf->setRawBuffer(mVar, pBuffer);

	// If you triggered this assert, your call 'myFSPass["someName"] = myBuffer' failed for various reasons.
	// You can comment it out if you don't mind that failed assignments will fail silently, without changing any state.
	assert(wasSet);
}

void SimpleVars::SharedPtr::Idx1::operator=(Falcor::TypedBufferBase::SharedPtr& pBuffer)
{
	// Set the buffer
	bool wasSet = mpBuf->setTypedBuffer(mVar, pBuffer);

	// If you triggered this assert, your call 'myFSPass["someName"] = myBuffer' failed for various reasons.
	// You can comment it out if you don't mind that failed assignments will fail silently, without changing any state.
	assert(wasSet);
}

void SimpleVars::SharedPtr::Idx1::operator=(Falcor::StructuredBuffer::SharedPtr& pBuffer)
{
	// Set the buffer
	bool wasSet = mpBuf->setStructuredBuffer(mVar, pBuffer);

	// If you triggered this assert, your call 'myFSPass["someName"] = myBuffer' failed for various reasons.
	// You can comment it out if you don't mind that failed assignments will fail silently, without changing any state.
	assert(wasSet);
}