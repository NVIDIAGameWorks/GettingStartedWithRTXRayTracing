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

#include "RayLaunch.h"

RayLaunch::SharedPtr RayLaunch::RayLaunch::create(const std::string &rayGenFile, const std::string& rayGenEntryPoint, int recursionDepth)
{
	return SharedPtr(new RayLaunch(rayGenFile, rayGenEntryPoint, recursionDepth));
}

RayLaunch::RayLaunch(const std::string &rayGenFile, const std::string& rayGenEntryPoint, int recursionDepth)
{
	mpRayState = RtState::create();
	mpRayState->setMaxTraceRecursionDepth(recursionDepth);
	mpRayProgDesc.addShaderLibrary(rayGenFile).setRayGen(rayGenEntryPoint);
	mpLastShaderFile = rayGenFile;

	mpRayProg = nullptr;
	mpRayVars = nullptr;
	mpSceneRenderer = nullptr;
	mpScene = nullptr;
	mInvalidVarReflector = true;
}

uint32_t RayLaunch::addMissShader(const std::string& missShaderFile, const std::string& missEntryPoint)
{
	if (mpLastShaderFile != missShaderFile)
		mpRayProgDesc.addShaderLibrary(missShaderFile);

	mpRayProgDesc.addMiss(mNumMiss, missEntryPoint);
	return mNumMiss++;
}

uint32_t RayLaunch::addHitShader(const std::string& hitShaderFile, const std::string& closestHitEntryPoint, const std::string& anyHitEntryPoint)
{
	if (mpLastShaderFile != hitShaderFile)
		mpRayProgDesc.addShaderLibrary(hitShaderFile);

	mpRayProgDesc.addHitGroup(mNumHitGroup, closestHitEntryPoint, anyHitEntryPoint);
	return mNumHitGroup++;
}

uint32_t RayLaunch::addHitGroup(const std::string& hitShaderFile, const std::string& closestHitEntryPoint, const std::string& anyHitEntryPoint, const std::string& intersectionEntryPoint)
{
	if (mpLastShaderFile != hitShaderFile)
		mpRayProgDesc.addShaderLibrary(hitShaderFile);

	mpRayProgDesc.addHitGroup(mNumHitGroup, closestHitEntryPoint, anyHitEntryPoint, intersectionEntryPoint);
	return mNumHitGroup++;
}

void RayLaunch::compileRayProgram()
{
	mpRayProg = RtProgram::create(mpRayProgDesc);
	mpRayState->setProgram(mpRayProg);
	mInvalidVarReflector = true;

	// Since generating ray tracing variables take a while, try to do it now.
	createRayTracingVariables();
}

bool RayLaunch::readyToRender()
{
	// Do we already know everything is ready?
	if (!mInvalidVarReflector && mpRayProg && mpRayVars) return true;

	// No?  Try creating our variable reflector.
	createRayTracingVariables();

	// See if we're ready now.
	return (mpRayProg && mpRayVars);
}

void RayLaunch::setMaxRecursionDepth(uint32_t maxDepth)
{
	if (mpRayState) mpRayState->setMaxTraceRecursionDepth(maxDepth);
	mInvalidVarReflector = true;
}

void RayLaunch::setScene(RtScene::SharedPtr pScene)
{
	// Make sure we have a valid scene 
	if (!pScene) return;
	mpScene = pScene;

	// Create a ray tracing renderer.
	mpSceneRenderer = RtSceneRenderer::create(mpScene);

	// Since the scene is an integral part of the variable reflector, we now need to update it!
	mInvalidVarReflector = true;

	// Since generating our ray tracing variables take quite a while (incl. build time), check if we can do it now
	if (mpRayProg) createRayTracingVariables();
}

void RayLaunch::addDefine(const std::string& name, const std::string& value)
{
	mpRayProg->addDefine(name, value);
	mInvalidVarReflector = true;
}

void RayLaunch::removeDefine(const std::string& name)
{
	mpRayProg->removeDefine(name);
	mInvalidVarReflector = true;
}

void RayLaunch::createRayTracingVariables()
{
	if (mpRayProg && mpScene)
	{
		mpRayVars = RtProgramVars::create(mpRayProg, mpScene);
		if (!mpRayVars) return;
		mInvalidVarReflector = false;

		// Generate the syntactic sugar wrappers to pass users of this class
		mpGlobalVars = SimpleVars::create(mpRayVars->getGlobalVars().get());
		mpRayGenVars = SimpleVars::create(mpRayVars->getRayGenVars().get());
	
		mpMissVars.clear();
		for (int i = 0; i<int(mNumMiss); i++)
		{
			mpMissVars.push_back(SimpleVars::create(mpRayVars->getMissVars(i).get()));
		}

		mpHitVars.clear();
		for (int i = 0; i<int(mNumHitGroup); i++)
		{
			int32_t curHitVarsIdx = int32_t(mpHitVars.size());

			SimpleVarsVector curVarVec;
			mpHitVars.push_back(curVarVec); 

			RtProgramVars::VarsVector curVars = mpRayVars->getHitVars(i);
			for (int j = 0; j < curVars.size(); j++)
			{
				auto instanceVar = curVars[j];
				mpHitVars[curHitVarsIdx].push_back(SimpleVars::create(instanceVar.get()));
			}
		}
	}
}

SimpleVars::SharedPtr RayLaunch::getGlobalVars()
{
	if (mInvalidVarReflector) createRayTracingVariables();
	return mpGlobalVars;
}

SimpleVars::SharedPtr RayLaunch::getRayGenVars()
{
	if (mInvalidVarReflector) createRayTracingVariables();
	return mpRayGenVars;
}

SimpleVars::SharedPtr RayLaunch::getMissVars(uint32_t rayType)
{
	if (mInvalidVarReflector) createRayTracingVariables();
	return (rayType >= uint32_t(mpMissVars.size())) ? nullptr : mpMissVars[rayType];
}

RayLaunch::SimpleVarsVector &RayLaunch::getHitVars(uint32_t rayType)
{
	if (mInvalidVarReflector) createRayTracingVariables();
	if (!mpRayVars || rayType >= uint32_t(mpHitVars.size())) return mDefaultHitVarList;

	return mpHitVars[rayType];
}

void RayLaunch::execute(RenderContext::SharedPtr pRenderContext, uvec2 rayLaunchDimensions, Camera::SharedPtr viewCamera)
{
    this->execute(pRenderContext.get(), rayLaunchDimensions, viewCamera);
}

void RayLaunch::execute(RenderContext* pRenderContext, uvec2 rayLaunchDimensions, Camera::SharedPtr viewCamera)
{
	// Ok.  We're executing.  If we still have an invalid shader variable reflector, we'd better get one now!
	if (mInvalidVarReflector) createRayTracingVariables();

	// We need our shader variable reflector in order to execute!
	if (!mpRayVars) return;

	// Get a camera pointer to pass to the renderer
	Camera *camPtr = nullptr;
	if (viewCamera)
		camPtr = viewCamera.get();
	else if (mpScene && mpScene->getActiveCamera())
		camPtr = mpScene->getActiveCamera().get();
	else
	{
		// No valid camera.  Launching ray tracing via renderScene() with a null camera may give undefined results!
		assert(false);
		return;
	}

	// Ok.  We're ready and have done all our error checking.  Launch the ray tracing!
	mpSceneRenderer->renderScene(pRenderContext, mpRayVars, mpRayState, uvec3(rayLaunchDimensions.x, rayLaunchDimensions.y, 1), camPtr);
}

void RayLaunch::experimentalExecute(RenderContext::SharedPtr pRenderContext, uvec2 rayLaunchDimensions)
{
	// We need our shader variable reflector in order to execute!
	if (!mpRayVars) return;

	// Ok.  We're ready and have done all our error checking.  Launch the ray tracing!
	mpSceneRenderer->renderScene(pRenderContext.get(), mpRayVars, mpRayState, uvec3(rayLaunchDimensions.x, rayLaunchDimensions.y, 1), nullptr);
}
