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

using namespace Falcor;

/** This is a very light wrapper around Falcor's raster rendering that removes boilerplate 
and uses the SimpleVars wrapper to access variables, constant buffers, textures, etc., using 
a simple array [] notation and overloaded operator=()

Initialization:
     RasterLaunch::SharedPtr mpRaster = RasterLaunch::createFromFiles("myVertex.vs.hlsl", "myFragment.ps.hlsl");
	 mpRaster->setScene( mpScene );

Pass setup / setting HLSL variable values:
     auto passHLSLVars = mpRaster->getVars();
     passHLSLVars["myShaderCB"]["myVar"] = uint4( 1, 2, 4, 16 );
     passHLSLVars["myShaderCB"]["myBinaryBlob"].setBlob( cpuSideBinaryBlob );
     passHLSLVars["myShaderTexture"] = myTextureResource;

Pass execution:
     mpRaster->execute( pRenderContext, pGraphicsPipelineState, pOutputFramebufferObject );

*/
class RasterLaunch : public std::enable_shared_from_this<RasterLaunch>
{
public:
	using SharedPtr = std::shared_ptr<RasterLaunch>;
	using SharedConstPtr = std::shared_ptr<const RasterLaunch>;
	virtual ~RasterLaunch() = default;

	// Create a raster pass from an existing Falcor GraphicsProgram object.  All other creation routines simply call this.
	static SharedPtr create(GraphicsProgram::SharedPtr &existingProgram);

	// Shortcut creation routines that take various filenames as input, then load the files and create a program
	static SharedPtr createFromFiles(const std::string& vertexFile, const std::string& fragmentFile);
	static SharedPtr createFromFiles(const std::string& vertexFile, const std::string& geometryFile, const std::string& fragmentFile);
	static SharedPtr createFromFiles(const std::string& vertexFile, const std::string& fragmentFile, const std::string& geometryFile, const std::string& hullFile, const std::string& domainFile);

	// If you use #define's in this pass' shaders and need to set them programmatically, use these methods (rather
	//     than built-in Falcor methods) to ensure setting resources via this class' syntactic sugar still works.
	// Note:  Treat updating #defines as invalidating all resources currently bound to the shaders.
	void addDefine(const std::string& name, const std::string& value);
	void removeDefine(const std::string& name);

	// When the Falcor scene you're using changes, make sure to tell us!
	void setScene(Scene::SharedPtr pScene);

	// Execute the shader.
	void execute(RenderContext::SharedPtr pRenderContext, GraphicsState::SharedPtr pGfxState, const Fbo::SharedPtr &pTargetFbo);
    void execute(RenderContext* pRenderContext, GraphicsState::SharedPtr pGfxState, const Fbo::SharedPtr &pTargetFbo);

	// Want to sent variables to your HLSL code, you do that via the SimpleVars structure
	SimpleVars::SharedPtr getVars();

protected:
	RasterLaunch(GraphicsProgram::SharedPtr &existingProgram);
	
	void createGraphicsVariables();

	GraphicsProgram::SharedPtr  mpPassShader;
	GraphicsVars::SharedPtr     mpSharedVars;
	SimpleVars::SharedPtr       mpSimpleVars;
	SceneRenderer::SharedPtr    mpSceneRenderer;
	bool                        mInvalidVarReflector;
};