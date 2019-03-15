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

/** This is a very light wrapper around Falcor's DirectX Raytracing API that removes lots of 
boilerplate and uses the SimpleVars wrapper to access variables, constant buffers, textures, 
etc., using a simple array [] notation and overloaded operator=()

Initialization:
     // Create wrapper, load ray generation shader, plus at least one miss shader and hit group
     RayLaunch::SharedPtr mpRays = RayLaunch::create("myRayTraceShader.hlsl", "RayGenerationFunctionName");
     mpRays->addMissShader("myMissShader.hlsl", "MissShaderFunctionName");              // Add miss shader #0
	 mpRays->addHitShader("myHitShaders.hlsl", "ClosestHitFuncName", "AnyHitFuncName"); // Add hit group #0

	 // (Optional) Add additional miss shaders and/or hit groups (index in HLSL increases for each additional call)
	 mpRays->addMissShader("myMissShader.hlsl", "AnotherMissShader");                                // Miss shader #1
	 mpRays->addMissShader("myMissShader.hlsl", "YetAnotherMissShader");                             // Miss shader #2
	 myRays->addHitShader("myHitShaders.hlsl", "ClosestHit2", "AnyHit2", "IntersectionShaderName" ); // Add hit group #1

	 // (Required) Add a reference to your scene and compile the programs
	 mpRays->compileRayProgram();
	 mpRays->setScene( mpScene );

Pass setup / setting HLSL variable values:

     // Set HLSL variables *only* visibile in the ray generation shader (in other shaders, they are undefined)
     auto rayGenVars = mpRays->getRayGenVars();
     rayGenVars["myShaderCB"]["myVar"] = uint4( 1, 2, 4, 16 );

	 // Set HLSL variables *only* visibile in the specific miss shader specified
     auto missVars = mpRays->getMissVars( missIdx );
	 missVars["envMap"] = myTextureResource;

	 // Set HLSL variables *only* visibile in the specific hit shader 
	 //   -> Note: for each hit group, there are *multiple* variable wrappers, one for each
	 //      geometry instance in your scene.  This allows you to specify per-instance
	 //      data that will be visibile in your hit or intersection shaders
	 for (auto pHitVars : mpRays->getHitVars( hitIdx ))
	 {
	      pHitVars["PerInstanceCB"]["objectColor"] = currentInstanceColor;
	 }

	 // Set HLSL variables visibile globally in *any* shader. 
	 //    NOTE: In DXR shaders in Falcor, HLSL global variables must be qualified with the 
	 //    "shared" keyword (as in:  'shared cbuffer myGlobalsCB { ... };')  This is 
	 //    non-standard HLSL, but without this, shared variables will be undefined. (Currently)
	 auto globalVars = mpRays->getGlobalVars();
	 globalVars["myGlobalsCB"]["gRayCount"]   = raysToShoot;
	 globalVars["myGlobalsCB"]["gRayEpsilon"] = minTValue;

Launch DirectX Raytracing ray generation shader:
     if (mpRays->readyToRender())  // Returns 'true' if mpRays has everything it needs
	      mpRays->execute( pRenderContext, uint2( rayLaunchWidth, rayLaunchHeight) );

*/

using namespace Falcor;

class RayLaunch : public std::enable_shared_from_this<RayLaunch>
{
public:
	using SharedPtr = std::shared_ptr<RayLaunch>;
	using SharedConstPtr = std::shared_ptr<const RayLaunch>;
	virtual ~RayLaunch() = default;

	static SharedPtr create(const std::string &rayGenFile, const std::string& rayGenEntryPoint, int recursionDepth=2);

	// Create a new miss shader
	uint32_t addMissShader(const std::string& missShaderFile, const std::string& missEntryPoint);

	// Create a new hit shader.  Either entry point can be the null string "" to not use a shader.  (But one of the two must be non-null)
	uint32_t addHitShader(const std::string& hitShaderFile, const std::string& closestHitEntryPoint, const std::string& anyHitEntryPoint);

	// NOTE: Advanced. Not fully tested all the way through Falcor's abstractions.  May not work as desired/expected. 
	//    Create a new hit group with closest hit, any-hit, and intersection shader. Use the null string "" for no shader.
	uint32_t addHitGroup(const std::string& hitShaderFile, const std::string& closestHitEntryPoint, const std::string& anyHitEntryPoint, const std::string& intersectionEntryPoint);

	// Call once you have added all the desired ray types
	void compileRayProgram();

	// Returns true if we have everything needed to call execute().
	bool readyToRender();

	// If you use #define's in this pass' shaders and need to set them programmatically, use these methods (rather
	//     than built-in Falcor methods) to ensure setting resources via this class' syntactic sugar still works.
	// Note:  Treat updating #defines as invalidating all resources currently bound to the shaders.
	void addDefine(const std::string& name, const std::string& value);
	void removeDefine(const std::string& name);

	// When the Falcor scene you're using changes, make sure to tell us!
	void setScene(RtScene::SharedPtr pScene);

	// Sets the max recursion depth (defaults to 2)
	void setMaxRecursionDepth(uint32_t maxDepth);

	// Launch our ray tracing with the specified number of rays.  If viewCamera is null, uses the scene's active camera
	void execute(RenderContext::SharedPtr pRenderContext, uvec2 rayLaunchDimensions, Camera::SharedPtr viewCamera = nullptr);
    void execute(RenderContext* pRenderContext, uvec2 rayLaunchDimensions, Camera::SharedPtr viewCamera = nullptr);

	// NOTE: Experimental functionality.  Probably do not use. Beware!
	void experimentalExecute(RenderContext::SharedPtr pRenderContext, uvec2 rayLaunchDimensions );

	// Get syntactic sugar to access global variables
	SimpleVars::SharedPtr getGlobalVars();

	// Get syntactic sugar to access ray gen variables
	SimpleVars::SharedPtr getRayGenVars();

	// Get syntactic sugar to access ray gen variables
	SimpleVars::SharedPtr getMissVars(uint32_t rayType);

	// Get syntacitc sugar to access hit shader variables
	using SimpleVarsVector = std::vector<SimpleVars::SharedPtr>;
	SimpleVarsVector &getHitVars(uint32_t rayType);

protected:
	RayLaunch(const std::string &rayGenFile, const std::string& rayGenEntryPoint, int recursionDepth=2);

	void createRayTracingVariables();

	RtProgram::SharedPtr          mpRayProg;        ///< Most abstract ray tracing pipeline (includes ray gen, miss, and hit shaders)
	RtProgram::Desc               mpRayProgDesc;   
	std::string                   mpLastShaderFile;
	uint32_t                      mNumMiss      = 0;
	uint32_t                      mNumHitGroup  = 0;

	RtProgramVars::SharedPtr      mpRayVars;        ///< Accessor / reflector for variables in all ray tracing shaders

	// These are defined to avoid regenerating them every time getRayGenVars(), etc., is called
	SimpleVars::SharedPtr               mpGlobalVars;
	SimpleVars::SharedPtr               mpRayGenVars;
	std::vector<SimpleVars::SharedPtr>  mpMissVars;
	std::vector<SimpleVarsVector>       mpHitVars;

	RtState::SharedPtr            mpRayState;
	RtSceneRenderer::SharedPtr    mpSceneRenderer;
	RtScene::SharedPtr            mpScene;
	bool                          mInvalidVarReflector = true;

	// Used only to return a zero-length list of hit shaders
	SimpleVarsVector mDefaultHitVarList;
};