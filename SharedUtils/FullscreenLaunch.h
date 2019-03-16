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
#include "SimpleVars.h"

/** This is a very light wrapper around Falcor::FullScreenPass that removes some of the boilerplate of
calling and initializing a FullScreenPass pass and uses the SimpleVars wrapper to access variables, 
constant buffers, textures, etc using a simple array [] notation and overloaded operator=()

Initialization:
   FullscreenLaunch::SharedPtr mpMyPass = FullscreenLaunch::create("myFullScreenShader.ps.hlsl");

Pass setup / setting HLSL variable values:
    auto passHLSLVars = mpMyPass->getVars();
	passHLSLVars["myShaderCB"]["myVar"] = uint4( 1, 2, 4, 16 );
	passHLSLVars["myShaderCB"]["myBinaryBlob"].setBlob( cpuSideBinaryBlob );
	passHLSLVars["myShaderTexture"] = myTextureResource;

Pass execution:
    pGraphicsState->setFbo( pOutputFbo );  
	mpMyPass->execute( pRenderContext, pGraphicsState );

*/
class FullscreenLaunch : public std::enable_shared_from_this<FullscreenLaunch>
{
public:
	using SharedPtr = std::shared_ptr<FullscreenLaunch>;
	using SharedConstPtr = std::shared_ptr<const FullscreenLaunch>;
	virtual ~FullscreenLaunch() = default;

	// Create our full-screen shader wrapper with a single HLSL fragment shader
	static SharedPtr create(const char *fragShader);

	// Execute the full-screen shader
	void execute(Falcor::RenderContext::SharedPtr pRenderContext, Falcor::GraphicsState::SharedPtr pGfxState);
    void execute(Falcor::RenderContext* pRenderContext, Falcor::GraphicsState::SharedPtr pGfxState);

	// Want to send variables to your HLSL code?  You do that via the SimpleVars wrapper
	SimpleVars::SharedPtr getVars();

	// Some internal Falcor state is not automatically set when using full-screen passes, in particular
	//     data related to the scene (since there is not necessarily a "scene" for a full-screen pass).
	//     If you want to use Falcor data like 'gCamera' and 'gLights[]' in HLSL, you can call these methods
	void setCamera(Falcor::Camera::SharedPtr pActiveCamera);
	void setLights(const std::vector< Falcor::Light::SharedPtr > &pLights);

	// Falcor allows programmatically adding #defines to your HLSL shader.  If you use this class, you
	//     should set them using the following methods (rather than default Falcor methods) to ensure
	//     the syntactic sugar for setting variables remains valid.
	// Note: When adding/removing defines, assume all previous HLSL variables you bound are invalidated
	void addDefine(const std::string& name, const std::string& value);
	void removeDefine(const std::string& name);

protected:
	FullscreenLaunch(const char *fragShader);

	// Called to recreate our variable reflectors when creating a program (or the old ones are invalidated)
	void createGraphicsVariables();

	bool                              mInvalidVarReflector = true;
	Falcor::FullScreenPass::UniquePtr mpPass;
	Falcor::GraphicsVars::SharedPtr   mpVars;
	SimpleVars::SharedPtr             mpSimpleVars;
};
