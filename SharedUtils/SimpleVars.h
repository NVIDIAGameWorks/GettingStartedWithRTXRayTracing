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

#include "Falcor.h"

/** This provides a clear syntactic sugar for sending varaibles to your HLSL shaders from C++ code

Usually usage of this class looks as follows:
    // Get access to your shaders's vars
	SimpleVars::SharedPtr hlslVars = myRasterPassWrapper->getVars(); 

	// Send your shader data down to the shader
    hlslVars["myShaderCB"]["myUint4Var"] = uint4( 1, 2, 4, 16 );
	hlslVars["myShaderCB"]["myFloatVar"] = float( 2.0 );
	hlslVars["myShaderCB"]["myStruct"].setBlob( myCpuCodeStruct );
	hlslVars["myTexture"] = myTextureResource;
	hlslVars["myBuffer"]  = myBufferResource;

This functionality is consistent across all wrappers and shader types that use SimpleVars.  You can
also use more Falcor-like syntax if you just want this class' beefed up error checking:
    hlslVars->setVariable("myShaderCB","myUint4Var", uint4(1, 2, 4, 16));
	hlslVars->setTexture("myTexture", myTextureResource);
	hlslVars->setTypedBuffer("myBuffer", myBufferResource);

The C++ code for this class, below, is ugly, confusing, and I'm not particularly proud of it.  
However, this syntactic sugar makes my coding, debugging, and experentation so much easier that
quite a number of people have decided to use this wrapper (or similar earlier versions I've written)

*/
class SimpleVars : public std::enable_shared_from_this<SimpleVars>
{
public:

	// This class uses an overloaded shared_ptr that allows calling array operations.
	class SharedPtr : public std::shared_ptr<SimpleVars>
	{
	public:

		// A secondary intermediary class that allows a double [][] operator to be used on the SharedPtr.
		class Var
		{
		public:
			// Constructor gets called when mySharedPtr["myIdx1"]["myVar"] is encountered
			Var(Falcor::ConstantBuffer *cb, const std::string& var) : mCB(cb), mVar(var) { if (cb) { mOffset = cb->getVariableOffset(var); } }

			// Assignment operator gets called when mySharedPtr["myIdx1"]["myVar"] = T(someData); is encountered
			template<typename T> void operator=(const T& val) { if (mOffset != Falcor::VariablesBuffer::kInvalidOffset) { mCB->setVariable(mOffset, val); } }

			// Allows mySharedPtr["myIdx1"]["myVar"].setBlob( blobData )...   In theory, block binary transfers could
			//      be done with operator=, but without careful coding that could *accidentally* do implicit binary transfers
			template<typename T> void setBlob(const T& blob) { if (mOffset != Falcor::VariablesBuffer::kInvalidOffset) { mCB->setBlob(&blob, mOffset, sizeof(T)); } }

			// A slightly more advanced setBlob() that allows the user to pass the size 
			//      explicitly (if it can't be deduced via sizeof(T))
			template<typename T> void setBlob(const T& blob, size_t blobSz) { if (mOffset != Falcor::VariablesBuffer::kInvalidOffset) { mCB->setBlob(&blob, mOffset, blobSz); } }
		protected:
			Falcor::ConstantBuffer *mCB;
			const std::string mVar;
			size_t mOffset = Falcor::VariablesBuffer::kInvalidOffset;
		};

		// An intermediary class that allows the [] operator to be used on the SharedPtr.
		class Idx1
		{
		public:
			// Constructor gets called when mySharedPtr["myIdx1"] is encoutered
			Idx1(SimpleVars* pBuf, const std::string& var) : mpBuf(pBuf), mVar(var) { }

			// When a second array operator is encountered, instatiate a Var object to handle mySharedPtr["myIdx1"]["myVar"]
			Var operator[](const std::string& var) { return mpBuf->mpVars ? Var(mpBuf->mpVars->getConstantBuffer(mVar).get(), var) : Var(nullptr, var); }

			// When encountering an assignment operator (i.e., mySharedPtr["myIdx1"] = pSomeResource;)  
			//   set the appropriate resource for the following types:  texture, sampler, various buffers
			//   Note: When compiling in Release mode, these fail silently if your specified shader resource
			//   is of the wrong type.  In Debug mode, you hit an assert in the appropriate operator=() so you
			//   can check which variable is screwed up.  TODO: Log errors rather than assert()ing.
			void operator=(const Falcor::Texture::SharedPtr& pTexture);            
			void operator=(const Falcor::Sampler::SharedPtr& pSampler);            
			void operator=(Falcor::TypedBufferBase::SharedPtr& pBuffer);
			void operator=(Falcor::StructuredBuffer::SharedPtr& pTexture);
			void operator=(Falcor::Buffer::SharedPtr& pTexture);

			// Allow conversion of this intermediary type to a constant buffer, e.g., for allowing:
			//     ConstantBuffer::SharedPtr cb = mySharedPtr["myIdx1"];
			operator Falcor::ConstantBuffer::SharedPtr() { return mpBuf->mpVars->getConstantBuffer(mVar); }

		protected:
			SimpleVars* mpBuf;
			const std::string mVar;
		};

		SharedPtr() = default;
		SharedPtr(SimpleVars* pBuf) : std::shared_ptr<SimpleVars>(pBuf) {}
		SharedPtr(std::shared_ptr<SimpleVars> pBuf) : std::shared_ptr<SimpleVars>(pBuf) {}

		// Calling [] on the SharedPtr?  Create an intermediate object to process further operators
		Idx1 operator[](const std::string& var) { return Idx1(get(), var); }
	};

	// public constructors
	static SharedPtr create( Falcor::Program::SharedPtr pProg );       // Create from a Falcor program
	static SharedPtr create( Falcor::GraphicsVars *pVars );  
	virtual ~SimpleVars() = default;

	// Set a variable
	template<typename T>
	void setVariable(const std::string& cBuf, const std::string& name, const T& value)
	{
		Falcor::ConstantBuffer::SharedPtr cb = mpVars->getConstantBuffer(cBuf);
		if (cb)
		{
			cb->setVariable(name, value);
		}
	}

	// Set a texture, sampler, or buffer resource.  (Could probably add setParameterBlock, too, if needed)
	//    -> Note:  These do additional error checks that core Falcor variants do not, which introduces 
	//       additional overhead in exchange for reduced hard-to-debug hard program crashes when a shader 
	//       name does not exist or refers to the wrong type.
	bool setTexture(const std::string& name, const Falcor::Texture::SharedPtr& pTexture);
	bool setSampler(const std::string& name, const Falcor::Sampler::SharedPtr& pSampler);
	bool setTypedBuffer(const std::string& name, Falcor::TypedBufferBase::SharedPtr& pBuffer);       
	bool setStructuredBuffer(const std::string& name, Falcor::StructuredBuffer::SharedPtr& pBuffer);
	bool setRawBuffer(const std::string& name, Falcor::Buffer::SharedPtr& pBuffer);

	// Get the current underlying Falcor variable class
	Falcor::GraphicsVars *getVars()
	{	
		return mpVars;
	}

protected:
	SimpleVars(Falcor::GraphicsVars *pVars);

private:
	Falcor::GraphicsVars*   mpVars = nullptr;

	// Internal utility function that does additional error checking beyond Falcor's built-in checks
	//    -> returns true if shader variable [varName] exists and has type [varType]
	bool isVarValid(const std::string &varName, Falcor::ReflectionResourceType::Type varType);
};
