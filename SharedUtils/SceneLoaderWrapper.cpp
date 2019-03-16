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

#include "SceneLoaderWrapper.h"

using namespace Falcor;

namespace {
    // Required for later versions of Falcor (post 3.1.0)
    //const FileDialogFilterVec kSceneExtensions = { {"fscene"} };
    //const FileDialogFilterVec kTextureExtensions = { { "hdr" }, { "png" }, { "jpg" }, { ".bmp" } };
};

Falcor::RtScene::SharedPtr loadScene( uvec2 currentScreenSize, const char *defaultFilename )
{
	RtScene::SharedPtr pScene;

	// If we didn't request a file to load, open a dialog box, asking which scene to load; on failure, return invalid scene
	std::string filename;
	if (!defaultFilename)
	{
        //if (!openFileDialog(kSceneExtensions, filename))
        if (!openFileDialog("All supported formats\0*.fscene\0Falcor scene (*.fscene)\0\0", filename))
			return pScene;
	}
	else
	{
		std::string fullPath;

		// Since we often run in Visual Studio, let's also check the relative paths to the binary directory...
		if (!findFileInDataDirectories(std::string(defaultFilename), fullPath))
			return pScene;

		filename = fullPath;
	}

	// Create a loading bar while loading a scene
	ProgressBar::SharedPtr pBar = ProgressBar::create("Loading Scene", 100);

	// Load a scene
	if (hasSuffix(filename, ".fscene", false))
	{
		pScene = RtScene::loadFromFile(filename, RtBuildFlags::None, Model::LoadFlags::RemoveInstancing);

		// If we have a valid scene, do some sanity checking; set some defaults
		if (pScene)
		{
			// Bind a sampler to all scene textures using linear filtering.  Note this is only used if 
			//    you use Falcor's built-in shading system.  Otherwise, you may have to specify your own sampler elsewhere.
			Sampler::Desc desc;
			desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
			Sampler::SharedPtr pSampler = Sampler::create(desc);
			pScene->bindSampler(pSampler);

			// Check to ensure the scene has at least one light.  If not, create a simple directional light
			if (pScene->getLightCount() == 0)
			{
				DirectionalLight::SharedPtr pDirLight = DirectionalLight::create();
				pDirLight->setWorldDirection(vec3(-0.189f, -0.861f, -0.471f));
				pDirLight->setIntensity(vec3(1, 1, 0.985f) * 10.0f);
				pDirLight->setName("DirLight");  // In case we need to display it in a GUI
				pScene->addLight(pDirLight);
			}

			// If scene doesn't have a camera, create one
			Camera::SharedPtr pCamera = pScene->getActiveCamera();
			if (!pCamera)
			{
				pCamera = Camera::create();

				// Set the created camera to a reasonable position based on scene bounding box.
				pCamera->setPosition(pScene->getCenter() + vec3(0, 0, 3.f * pScene->getRadius()));
				pCamera->setTarget(pScene->getCenter());
				pCamera->setUpVector(vec3(0, 1, 0));
				pCamera->setDepthRange(std::max(0.1f, pScene->getRadius() / 750.0f), pScene->getRadius() * 10.f);

				// Attach the camera to the scene (as the active camera); change camera motion to something reasonable
				pScene->setActiveCamera(pScene->addCamera(pCamera));
				pScene->setCameraSpeed(pScene->getRadius() * 0.25f);
			}

			// Set the aspect ratio of the camera appropriately
			pCamera->setAspectRatio((float)currentScreenSize.x / (float)currentScreenSize.y);

			// If scene has a camera path, disable from starting animation at load.
			if (pScene->getPathCount())
				pScene->getPath(0)->detachObject(pScene->getActiveCamera());
		}
	}

	// We're done.  Return whatever scene we might have
	return pScene;
}

std::string getTextureLocation(bool &isValid)
{
	// Open a dialog box, asking which scene to load; on failure, return invalid scene
	std::string filename;
    if (!openFileDialog("All supported formats\0*.hdr;*.png;*.jpg;*.bmp\0\0", filename))
	//if (!openFileDialog(kTextureExtensions, filename))
	{
		isValid = false;
		return std::string("");
	}

	isValid = true;
	return filename;
}