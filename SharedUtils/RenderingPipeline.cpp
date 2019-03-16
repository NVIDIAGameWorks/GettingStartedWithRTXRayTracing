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

#include "Falcor.h"
#include "RenderingPipeline.h"
#include "Externals/dear_imgui/imgui.h"
#include "SceneLoaderWrapper.h"
#include <algorithm>

namespace {
	const char     *kNullPassDescriptor = "< None >";   ///< Name used in dropdown lists when no pass is selected.
	const uint32_t  kNullPassId = 0xFFFFFFFFu;          ///< Id used to represent the null pass (using -1).
};


RenderingPipeline::RenderingPipeline() 
	: Renderer()
{
}

uint32_t RenderingPipeline::addPass(::RenderPass::SharedPtr pNewPass)
{
	size_t id = mAvailPasses.size();
	mAvailPasses.push_back(pNewPass);
	return uint32_t(id);
}

void RenderingPipeline::onLoad(SampleCallbacks* pSample, const RenderContext::SharedPtr &pRenderContext)
{
	// Give the GUI some heft, so we don't need to resize all the time
	pSample->setDefaultGuiSize(300, 800);

	// Create our resource manager
	mpResourceManager = ResourceManager::create(mLastKnownSize.x, mLastKnownSize.y, pSample);
	mOutputBufferIndex = mpResourceManager->requestTextureResource(ResourceManager::kOutputChannel);

	// Initialize all of the RenderPasses we have available to select for our pipeline
	for (uint32_t i = 0; i < mAvailPasses.size(); i++)
	{
		if (mAvailPasses[i])
		{
			// Initialize.  If failure, remove this pass from the list.
			bool initialized = mAvailPasses[i]->onInitialize(pRenderContext.get(), mpResourceManager);
			if (!initialized) mAvailPasses[i] = nullptr;
		}
	}

    // If nobody has started inserting passes into our pipeline, set up our GUI so we can start adding passes manually.
	if (mActivePasses.size() == 0)
	{
		insertPassIntoPipeline(0);
	}

    // Initialize GUI pass selection dropdown lists.
    for (uint32_t i = 0; i < mPassSelectors.size(); i++)
    {
		if (mPassSelectors[i].size() <= 0)
			createDefaultDropdownGuiForPass(i, mPassSelectors[i]);
    }

	// Create identifiers for profiling.
    mProfileGPUTimes.resize(mActivePasses.size() * 2);
    mProfileLastGPUTimes.resize(mActivePasses.size() * 2);
	for (uint32_t i = 0; i < mActivePasses.size()*2; i++)
	{
		char buf[256];
		sprintf_s(buf, "Pass_%d", i);
		mProfileNames.push_back( HashedString(std::string(buf)) );
	}

	// Create a camera controller
	mpCameraControl = CameraController::SharedPtr(new FirstPersonCameraController);
	mpCameraControl->attachCamera(nullptr);

	// We're going to create a default graphics state
	mpDefaultGfxState = GraphicsState::create();

	// If we've requested to have an environment map... 
	if (mPipeUsesEnvMap)
	{
		// Check to see if someone else already loaded one. 
		std::string envName = mpResourceManager->getEnvironmentMapName();
		if (envName == "") // No loaded map?  Create a default one
		{
			mpResourceManager->updateEnvironmentMap("");
			mEnvMapSelector.push_back({ 0, "Sky blue (i.e., [0.5, 0.5, 0.8])" });
		}
		else  // Map already loaded, get it's name to put in the UI
			mEnvMapSelector.push_back({ 0, envName.c_str() });

		// Add a UI option to load a new HDR environment map
		mEnvMapSelector.push_back({ 1, "< Load new map... >" });
		mEnvMapSelector.push_back({ 2, "Switch -> black environment"  });
		mEnvMapSelector.push_back({ 3, "Switch -> sky blue environment" });

		if(findFileInDataDirectories("MonValley_G_DirtRoad_3k.hdr", mMonValleyFilename))
		{
			mHasMonValley = true;
			mEnvMapSelector.push_back({ 4, "Switch -> desert HDR environment" });
		}

	}

	// Set the samples freeze-time setting appropriately
	pSample->freezeTime(mFreezeTime);

	// When we initialize, we have a new pipe, so we need to give data to the passes
	updatePipelineRequirementFlags();
	mPipelineChanged = true;

	// Note that we've done our initialzation pass
	mIsInitialized = true;

    return;
}

void RenderingPipeline::updatePipelineRequirementFlags(void)
{
	// Start by changing all flags to false
	mPipeRequiresScene = false;
	mPipeRequiresRaster = false;
	mPipeRequiresRayTracing = false;
	mPipeAppliesPostprocess = false;
	mPipeUsesCompute = false;
	mPipeUsesEnvMap = false;
	mPipeNeedsDefaultScene = mpResourceManager ? mpResourceManager->userSetDefaultScene() : false;
	mPipeHasAnimation = false;

	for (uint32_t passNum = 0; passNum < mActivePasses.size(); passNum++)
	{
		if (mActivePasses[passNum])
		{
			mPipeRequiresScene = mPipeRequiresScene || mActivePasses[passNum]->requiresScene();
			mPipeRequiresRaster = mPipeRequiresRaster || mActivePasses[passNum]->usesRasterization();
			mPipeRequiresRayTracing = mPipeRequiresRayTracing || mActivePasses[passNum]->usesRayTracing();
			mPipeAppliesPostprocess = mPipeAppliesPostprocess || mActivePasses[passNum]->appliesPostprocess();
			mPipeUsesCompute = mPipeUsesCompute || mActivePasses[passNum]->usesCompute();
			mPipeUsesEnvMap = mPipeUsesEnvMap || mActivePasses[passNum]->usesEnvironmentMap();
			mPipeNeedsDefaultScene = mPipeNeedsDefaultScene || mActivePasses[passNum]->loadDefaultScene();
			mPipeHasAnimation = mPipeHasAnimation || mActivePasses[passNum]->hasAnimation();
		}
	}

	mPipeRequiresScene = mPipeRequiresScene || mPipeNeedsDefaultScene;
}

void RenderingPipeline::createDropdownGuiForPass(uint32_t passOrder, Gui::DropdownList& outputList)
{
	// Ensure our list is empty
	outputList.resize(0);

	// Add an item to allow selecting a null pass.
	outputList.push_back({ int32_t(kNullPassId), kNullPassDescriptor });

	//outputList.push_back({ int32_t(passOrder), mAvailPasses[passOrder]->getName() });
	
	// Include an item in the dropdown for each possible pass
	for (uint32_t i = 0; i < mAvailPasses.size(); i++)
	{
		// Don't add if this pass doesn't exist or if we can't insert it at that location in the pipeline.
		if (!mAvailPasses[i]) continue;
		if (!isPassValid(mAvailPasses[i], passOrder)) continue;

		// Ok.  The pass exists and is able to be selected for pass number <passOrder>.  Insert it into the list.
		outputList.push_back({ int32_t(i), mAvailPasses[i]->getName() });
	}
	
}

void RenderingPipeline::createDefaultDropdownGuiForPass(uint32_t passOrder, Gui::DropdownList& outputList)
{
	// Ensure our list is empty
	outputList.resize(0);

	// Add an item to allow selecting a null pass.
	outputList.push_back({ int32_t(kNullPassId), kNullPassDescriptor });

	//outputList.push_back({ int32_t(passOrder), mAvailPasses[passOrder]->getName() });

	// Include an item in the dropdown for each possible pass
	for (uint32_t i = 0; i < mAvailPasses.size(); i++)
	{
		// Don't add if this pass doesn't exist or if we can't insert it at that location in the pipeline.
		if (!mAvailPasses[i]) continue;
		if (!isPassValid(mAvailPasses[i], passOrder)) continue;

		// Ok.  The pass exists and is able to be selected for pass number <passOrder>.  Insert it into the list.
		outputList.push_back({ int32_t(i), mAvailPasses[i]->getName() });
	}
}

// Returns true if pCheckPass is valid to insert in the pass sequence at location <passNum>
bool RenderingPipeline::isPassValid(::RenderPass::SharedPtr pCheckPass, uint32_t passNum)
{
	// For now, say that all passes can be inserted everywhere...
	return true;
}

void RenderingPipeline::onGuiRender(SampleCallbacks* pSample, Gui* pGui)
{
    //Falcor::ProfilerEvent _profileEvent("renderGUI");

	pGui->addSeparator();

	// Add a button to allow the user to load a scene
	if (mPipeRequiresScene)
	{
		pGui->addText("Need to open a new scene?  Click below:");
		pGui->addText("     ");
		if (pGui->addButton("Load Scene", true))
		{
			// A wrapper function to open a window, load a UI, and do some sanity checking
			Fbo::SharedPtr outputFBO = pSample->getCurrentFbo();
			RtScene::SharedPtr loadedScene = loadScene(uvec2(outputFBO->getWidth(), outputFBO->getHeight()));

			// We have a method that explicitly initializes all render passes given our new scene.
			if (loadedScene)
			{
				onInitNewScene(pSample->getRenderContext().get(), loadedScene);
				mGlobalPipeRefresh = true;
			}
		}
		pGui->addSeparator();
	}

	if (mPipeUsesEnvMap)
	{
		uint32_t selection = 0;
		pGui->addText( "Current environment map:" );
		pGui->addText("     ");
		if (pGui->addDropdown("##envMapSelector", mEnvMapSelector, selection, true))
		{
			if (selection == 1)  // Then we've asked to load a new map
			{
				bool isValid = false;
				std::string fileName = getTextureLocation(isValid);
				if (isValid && mpResourceManager->updateEnvironmentMap(fileName))
				{
					mEnvMapSelector[0] = { 0, mpResourceManager->getEnvironmentMapName().c_str() };
				}
			}
			else if (selection == 2)  // select default "black" environment
			{
				// Choose the black background
				mpResourceManager->updateEnvironmentMap("Black");
				mEnvMapSelector[0] = { 0, "Black (i.e., [0.0, 0.0, 0.0])" };
			}
			else if (selection == 3)  // select default "sky blue" environment
			{
				mpResourceManager->updateEnvironmentMap("");
				mEnvMapSelector[0] = { 0, "Sky blue (i.e., [0.5, 0.5, 0.8])" };
			}
			else if (selection == 4)
			{
				mEnvMapSelector[0] = { 0, "Desert HDR environment" };
				mpResourceManager->updateEnvironmentMap(mMonValleyFilename);
			}
			mGlobalPipeRefresh = true;
		}
		pGui->addSeparator();
	}

	if (mPipeRequiresRayTracing && mpResourceManager)
	{
		pGui->addText("Set ray tracing min traversal distance:");
		pGui->addText("     ");
		if (pGui->addDropdown("##minTSelector", mMinTDropdown, mMinTSelection, true))
		{
			mpResourceManager->setMinTDist(mMinTArray[mMinTSelection]);
			mGlobalPipeRefresh = true;
		}
		pGui->addSeparator();
	}

	// To avoid putting GUIs on top of each other, offset later passes
	int yGuiOffset = 0;

	// Do we have pipeline instructions?
	if (mPipeDescription.size() > 0)
	{
		// Print all the lines in the instructions / help message
		for (auto line : mPipeDescription)
		{
			pGui->addText(line.c_str());
		}

		// Add a blank line.
		pGui->addText(""); 
	}

	pGui->addText("");
	pGui->addText("Ordered list of passes in rendering pipeline:");
	pGui->addText("       (Click the boxes at left to toggle GUIs)");
	for (uint32_t i = 0; i < mPassSelectors.size(); i++)
	{
		char buf[128];

		// Draw a button that enables/disables showing this pass' GUI window
		sprintf_s(buf, "##enable.pass.%d", i);
		bool enableGui = mEnablePassGui[i];
		if (pGui->addCheckBox(buf, enableGui)) mEnablePassGui[i] = enableGui;

		// Draw the selectable list of passes we can add at this stage in the pipeline
		sprintf_s(buf, "##selector.pass.%d", i);
		if (pGui->addDropdown(buf, mPassSelectors[i], mPassId[i], true))
		{
			::RenderPass::SharedPtr selectedPass = (mPassId[i] != kNullPassId) ? mAvailPasses[mPassId[i]] : nullptr;
			changePass(i, selectedPass);
		}

		// If the GUI for this pass is enabled, go ahead and draw the GUI
		if (mEnablePassGui[i] && mActivePasses[i])
		{
			// Find out where to put the GUI for this pass
			ivec2 guiPos = mActivePasses[i]->getGuiPosition();
			ivec2 guiSz = mActivePasses[i]->getGuiSize();

			// If the GUI position is negative, attach to right/bottom on screen
			guiPos.x = (guiPos.x < 0) ? (mLastKnownSize.x + guiPos.x) : guiPos.x;
			guiPos.y = (guiPos.y < 0) ? (mLastKnownSize.y + guiPos.y) : guiPos.y;

			// Offset the positions of the GUIs depending on where they are in the pipeline.  If we move it down too far
			//    due to number of passes, give up and draw it just barely visibile at the bottom of the screen.
			guiPos.y += yGuiOffset; 
			guiPos.y = glm::min(guiPos.y, int(mLastKnownSize.y) - 100);

			// Create a window.  Note: RS4 version does more; that doesn't work with recent Falcor; this is OK for just tutorials.
            pGui->pushWindow(mActivePasses[i]->getGuiName().c_str(),guiSz.x, guiSz.y, guiPos.x, guiPos.y, true, true);

			// Render the pass' GUI to this new UI window, then pop the new UI window.
			mActivePasses[i]->onRenderGui(pGui);
			pGui->popWindow();
		}

		// Offset the next GUI by the current one's height
		if (mActivePasses[i])
			yGuiOffset += mActivePasses[i]->getGuiSize().y; 
	}

	pGui->addText("");

	// Enable an option to enable/disable binding of the camera to a path
	if (mpScene && mpScene->getPathCount() && mPipeHasAnimation)
	{
		if (pGui->addCheckBox("Animated camera path?", mUseSceneCameraPath))
		{
			if (mUseSceneCameraPath)
			{
				mpScene->getPath(0)->attachObject(mpScene->getActiveCamera());
			}
			else
			{
				mpScene->getPath(0)->detachObject(mpScene->getActiveCamera());
			}
		}
	}

	// Allow control over any scene animation
	if (mPipeHasAnimation)
	{
		if (pGui->addCheckBox("Freeze all scene animations", mFreezeTime))
		{
			pSample->freezeTime(mFreezeTime);
		}
	}

    pGui->addText("");
    pGui->addSeparator();
    pGui->addText(Falcor::gProfileEnabled ? "Press (P):  Hide profiling window" : "Press (P):  Show profiling window");
    pGui->addSeparator();
}

void RenderingPipeline::removePassFromPipeline(uint32_t passNum)
{
	// Check index validity (and don't allow removal of the last list entry)
	if (passNum >= mActivePasses.size() - 1)
	{
		return;
	}

	// If we're removing an active pass, deactive the current one.
	if (mActivePasses[passNum])
	{
		// Tell the pass that it's been deactivated
		mActivePasses[passNum]->onPassDeactivation();
	}

	// Remove entry from all the internal lists
	mActivePasses.erase(mActivePasses.begin() + passNum);
	mPassSelectors.erase(mPassSelectors.begin() + passNum);
	mPassId.erase(mPassId.begin() + passNum);
	mEnablePassGui.erase(mEnablePassGui.begin() + passNum);
	mEnableAddRemove.erase(mEnableAddRemove.begin() + passNum);

	// (Re)-create a GUI selector for all passes (after the removed one)  
	for (uint32_t i = 0; i < mPassSelectors.size(); i++)
	{
		if (mPassSelectors[i].size() <= 0)
			createDefaultDropdownGuiForPass(i, mPassSelectors[i]);
	}

	// The pipeline has changed, so make sure to let people know.
	mPipelineChanged = true;
}

void RenderingPipeline::setPass(uint32_t passNum, ::RenderPass::SharedPtr pTargetPass, bool canAddPassAfter, bool canRemovePass)
{
    // If we're setting pass after last pass in the list, insert null passes so that all slots are valid.
    for (uint32_t i = (uint32_t)mPassId.size(); i <= passNum; i++)
    {
        insertPassIntoPipeline(i);
    }

    // Get unique pass index. Add pass to list of available passes.
    uint32_t passIdx = kNullPassId;
    if (pTargetPass)
    {
        // Find if this pass is in our existing list of available passes.
        auto passLoc = std::find(mAvailPasses.begin(), mAvailPasses.end(), pTargetPass);
        passIdx = uint32_t(passLoc - mAvailPasses.begin());

        // If it is not in the list of available passes, then add it.
        if (passLoc == mAvailPasses.end())
        {
			passIdx = addPass(pTargetPass);
        }
    }

	// Create a GUI dropdown for this new pass
	mPassSelectors[passNum].resize(0);
	mPassSelectors[passNum].push_back({ int32_t(kNullPassId), kNullPassDescriptor });
	if (pTargetPass && passIdx != kNullPassId)
	{
		mPassSelectors[passNum].push_back({ int32_t(passIdx), mAvailPasses[passIdx]->getName() });
	}

    // Update the settings for the specified active pass.
    mPassId[passNum] = passIdx;
    mEnableAddRemove[passNum] = (canAddPassAfter ? UIOptions::CanAddAfter : 0x0u) | (canRemovePass ? UIOptions::CanRemove : 0x0u);

    if (mIsInitialized)
    {
        // If setPass() was called after initialization, call changePass() to properly active/deactive passes.
        changePass(passNum, pTargetPass);
    }
    else
    {
        // If not initialized, just store the pass ptr. It will be resized/actived later.
        // We do this way so that callbacks are not called on the pass before the device is created.
        // The initialize() function should always be the first to be called.
        mActivePasses[passNum] = pTargetPass;
    }

	// The pipeline has changed, so set the flag.
	mPipelineChanged = true;
}

void RenderingPipeline::setPassOptions(uint32_t passNum, std::vector<::RenderPass::SharedPtr> pPassList)
{
	// If we're setting pass after last pass in the list, insert null passes so that all slots are valid.
	for (uint32_t i = (uint32_t)mPassId.size(); i <= passNum; i++)
	{
		insertPassIntoPipeline(i);
	}

	// Can't do anything else if the list of passes to choose from for this pass is null-length
	if (pPassList.size() <= 0) return;

	// Create a GUI dropdown for this new pass
	mPassSelectors[passNum].resize(0);
	mPassSelectors[passNum].push_back({ int32_t(kNullPassId), kNullPassDescriptor });

	// Update the settings for the specified active pass.
	mEnableAddRemove[passNum] = 0x0u; 

	uint32_t passIdx = kNullPassId;
	for (uint32_t i = 0; i < uint32_t(pPassList.size()); i++)
	{
		// Get unique pass index. Add pass to list of available passes.
		if (pPassList[i])
		{
			// Find if this pass is in our existing list of available passes.
			auto passLoc = std::find(mAvailPasses.begin(), mAvailPasses.end(), pPassList[i]);
			passIdx = uint32_t(passLoc - mAvailPasses.begin());

			// If it is not in the list of available passes, then add it.
			if (passLoc == mAvailPasses.end())
			{
				passIdx = addPass(pPassList[i]);
			}

			if (passIdx != kNullPassId)
			{
				mPassSelectors[passNum].push_back({ int32_t(passIdx), mAvailPasses[passIdx]->getName() });
			}
		}

		// Set the active pass to be the first one in the list
		if (i==0) mPassId[passNum] = passIdx;
	}

	if (mIsInitialized)
	{
		// If setPass() was called after initialization, call changePass() to properly active/deactive passes.
		changePass(passNum, pPassList[0]);
	}
	else
	{
		// If not initialized, just store the pass ptr. It will be resized/actived later.
		// We do this way so that callbacks are not called on the pass before the device is created.
		// The initialize() function should always be the first to be called.
		mActivePasses[passNum] = pPassList[0];
	}

	// The pipeline has changed, so set the flag.
	mPipelineChanged = true;
}

void RenderingPipeline::insertPassIntoPipeline(uint32_t afterPass)
{
	// Since std::vector::insert() inserts *before* the specified element, we need to use afterPass+1.  
	//    But if inserting into an empty list (or potentially at the end of a list), this causes problems,
	//    so do some index clamping.
	uint32_t insertLoc = (afterPass < mActivePasses.size()) ? afterPass + 1 : uint32_t(mActivePasses.size());

	// Insert new null entry into the list
	Gui::DropdownList nullSelector;
	mActivePasses.insert(mActivePasses.begin() + insertLoc, nullptr);
	mPassSelectors.insert(mPassSelectors.begin() + insertLoc, nullSelector);
	mPassId.insert(mPassId.begin() + insertLoc, kNullPassId);
	mEnablePassGui.insert(mEnablePassGui.begin() + insertLoc, false);
	mEnableAddRemove.insert(mEnableAddRemove.begin() + insertLoc, int32_t( UIOptions::CanAddAfter | UIOptions::CanRemove ) );

	// (Re)-create a GUI selector for all passes (including and after the new pass)  
	for (uint32_t i = insertLoc; i < mPassSelectors.size(); i++)
	{
		if (mPassSelectors[i].size() <= 0)
			createDefaultDropdownGuiForPass(i, mPassSelectors[i]);
	}
}

void RenderingPipeline::changePass(uint32_t passNum, ::RenderPass::SharedPtr pNewPass)
{
    // Early out if the new pass is the same as the old pass.
    if (mActivePasses[passNum] && mActivePasses[passNum] == pNewPass)
    {
        return;
    }

	// If we're changing an active pass to a different one, deactivate the current pass
	if (mActivePasses[passNum])
	{
		// Tell the pass that it's been deactivated
		mActivePasses[passNum]->onPassDeactivation();
	}

	// Set the selected active pass
	mActivePasses[passNum] = pNewPass;

	// Do any activation of the newly selected pass (if it's non-null)
	if (pNewPass)
	{
		// Ensure the pass knows the correct size
		pNewPass->onResize(mLastKnownSize.x, mLastKnownSize.y);

		// Tell the pass that it's been activated
		pNewPass->onPassActivation();
	}

    // (Re)-create a GUI selector for all passes (including any newly added one)
    //    We recreate all selectors in case the pass change we're now processing
    //    affects the validity of selections in previously-created lists.
	for (uint32_t i = 0; i < mPassSelectors.size(); i++)
	{
		if (mPassSelectors[i].size() <= 0)
			createDefaultDropdownGuiForPass(i, mPassSelectors[i]);
	}

	// If we've changed a pass, the pipeline has changed
	updatePipelineRequirementFlags();
	mPipelineChanged = true;
}

void RenderingPipeline::onFirstRun(SampleCallbacks* pSample)
{
	// Did the user ask for us to load a scene by default?
	if (mPipeNeedsDefaultScene)
	{
		RtScene::SharedPtr loadedScene = loadScene(mLastKnownSize, mpResourceManager->getDefaultSceneName().c_str());
		if (loadedScene) onInitNewScene(pSample->getRenderContext().get(), loadedScene);
	}

	mFirstFrame = false;
}

void RenderingPipeline::onFrameRender(SampleCallbacks* pSample, const RenderContext::SharedPtr &pRenderContext, const Fbo::SharedPtr &pTargetFbo)
{
	// Is this the first time we've run onFrameRender()?  If som take care of things that happen on first execution.
	if (mFirstFrame) onFirstRun(pSample);

	// Bind our default state to the graphics pipe
	pRenderContext->pushGraphicsState(mpDefaultGfxState);

	// Check to ensure we have all our resources initialized.  (This should be superfluous) 
	if (!mpResourceManager->isInitialized())
	{
		mpResourceManager->initializeResources();
	}

	// If we have a scene, make sure to update the current camera based on any UI controls
	if (mpScene)
	{
		// Make sure we're updateing the correct camera, then update the scene
		mpCameraControl->attachCamera(mpScene->getActiveCamera() ? mpScene->getActiveCamera() : nullptr);
		mpScene->update(pSample->getCurrentTime(), mpCameraControl.get());
	}

	// Check if the pipeline has changed since last frame and needs updating
	bool updatedPipeline = false;
	if (anyRequestedPipelineChanges())
	{
		// If there's a change, let all the passes know
		for (uint32_t passNum = 0; passNum < mActivePasses.size(); passNum++)
		{
			if (mActivePasses[passNum])
			{
				mActivePasses[passNum]->onPipelineUpdate( mpResourceManager );
			}
		}

		// Update our flags
		updatePipelineRequirementFlags();
		updatedPipeline = true;
	}

	// Check if any passes have set their refresh flag
	if (havePassesSetRefreshFlag() || updatedPipeline || mGlobalPipeRefresh)
	{
		// If there's a change, let all the passes know
		for (uint32_t passNum = 0; passNum < mActivePasses.size(); passNum++)
		{
			if (mActivePasses[passNum])
			{
				mActivePasses[passNum]->onStateRefresh();
			}
		}
		mGlobalPipeRefresh = false;
	}

    // Execute all of the passes in the current pipeline
    for (uint32_t passNum = 0; passNum < mActivePasses.size(); passNum++)
    {
        if (mActivePasses[passNum])
        {
            if (Falcor::gProfileEnabled)
            {
                // Insert a per-pass profiling event.  
                assert(passNum < mProfileNames.size());
                Falcor::ProfilerEvent _profileEvent(mActivePasses[passNum]->getName().c_str());
                mActivePasses[passNum]->onExecute(pRenderContext.get());
            }
            else
            {
                mActivePasses[passNum]->onExecute(pRenderContext.get());
            }
        }
    }

	// Now that we're done rendering, grab out output texture and blit it into our target FBO
	if (pTargetFbo && mpResourceManager->getTexture(mOutputBufferIndex))
	{
		pRenderContext->blit(mpResourceManager->getTexture(mOutputBufferIndex)->getSRV(), pTargetFbo->getColorTexture(0)->getRTV());
	}

	// Once we're done rendering, clear the pipeline dirty state.
	mPipelineChanged = false;

	// Get rid of our default state
	pRenderContext->popGraphicsState();
}

void RenderingPipeline::onInitNewScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash the scene in the pipeline
	if (pScene) 
		mpScene = pScene;

	// When a new scene is loaded, we'll tell all our passes about it (not just active passes)
	for (uint32_t i = 0; i < mAvailPasses.size(); i++)
	{
		if (mAvailPasses[i])
		{
			mAvailPasses[i]->onInitScene(pRenderContext, pScene);
		}
	}
}

void RenderingPipeline::onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height)
{
	// Stash the current size, so if we need it later, we'll have access.
	mLastKnownSize = uvec2(width, height);

	// If we're getting zeros for width or height, we're don't have a real screen yet and we're
	//    going to get lots of resource resizing issues.  Stop until we have a reasonable size
	if (width <= 0 || height <= 0) return;

	// Resizes our resource manager
	if (mpResourceManager)
	{
		mpResourceManager->resize(width, height);
	}

	// We're only going to resize render passes that are active.  Other passes will get resized when activated.
	for (uint32_t i = 0; i < mActivePasses.size(); i++)
	{
		if (mActivePasses[i])
		{
			mActivePasses[i]->onResize(width, height);
		}
	}
}

void RenderingPipeline::onShutdown(SampleCallbacks* pSample)
{
	// On program shutdown, call the shutdown callback on all the render passes.
    // We do not have to worry about double-deletion etc. It is currently enforced that a pass is only bound to one pipeline.
	for (uint32_t i = 0; i < mAvailPasses.size(); i++)
	{
		if (mAvailPasses[i])
		{
			mAvailPasses[i]->onShutdown();
		}
	}
}

bool RenderingPipeline::onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent)
{
	// Let all of the currently active render passes process any keyboard events
	for (uint32_t i = 0; i < mActivePasses.size(); i++)
	{
		if (mActivePasses[i] && mActivePasses[i]->onKeyEvent(keyEvent))
		{
			return true;
		}
	}

	return mpCameraControl->onKeyEvent(keyEvent);
}

bool RenderingPipeline::onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent)
{
	// Some odd cases where this gets called by Falcor error message boxes.  Ignore these.
	if (!pSample || !mpCameraControl) return false;

	// Let all of the currently active render passes process any mouse events
	for (uint32_t i = 0; i < mActivePasses.size(); i++)
	{
		if (mActivePasses[i] && mActivePasses[i]->onMouseEvent(mouseEvent))
		{
			return true;
		}
	}

	return mpCameraControl->onMouseEvent(mouseEvent);
}

bool RenderingPipeline::canRemovePass(uint32_t passNum)
{
	if (passNum >= mEnableAddRemove.size()) return false;
	return (mEnableAddRemove[passNum] & UIOptions::CanRemove) != 0x0u;
}

bool RenderingPipeline::canAddPassAfter(uint32_t passNum)
{
	if (passNum >= mEnableAddRemove.size()) return false;
	return (mEnableAddRemove[passNum] & UIOptions::CanAddAfter) != 0x0u;
}

void RenderingPipeline::getActivePasses(std::vector<::RenderPass::SharedPtr>& activePasses) const
{
    activePasses.clear();
    for (auto& pPass : mActivePasses)
    {
        if (pPass)
        {
            activePasses.push_back(pPass);
        }
    }
}

bool RenderingPipeline::anyRequestedPipelineChanges(void)
{
	// Ask our passes if they've change the pipeline
	for (uint32_t passNum = 0; passNum < mActivePasses.size(); passNum++)
	{
		if (mActivePasses[passNum] && mActivePasses[passNum]->isRebindFlagSet())
		{
			mPipelineChanged = true;
			mActivePasses[passNum]->resetRebindFlag();
		}
	}

	// Ask our resource manager if it has changed any pipeline state
	if (mpResourceManager->haveResourcesChanged())
	{
		mPipelineChanged = true;
		mpResourceManager->resetDirtyFlag();
	}

	return mPipelineChanged;
}

bool RenderingPipeline::havePassesSetRefreshFlag(void)
{
	bool refreshFlag = false;
	for (uint32_t passNum = 0; passNum < mActivePasses.size(); passNum++)
	{
		if (mActivePasses[passNum] && mActivePasses[passNum]->isRefreshFlagSet())
		{
			refreshFlag = true;
		}
	}
	return refreshFlag;
}

void RenderingPipeline::addPipeInstructions(const std::string &str)
{
	mPipeDescription.push_back(str);
}

void RenderingPipeline::extractProfilingData(void)
{
	// This is a pretty ugly method.  It basically undoes Falcor's standard
	//    profiler output, which is a strigified concatenation of all the data
	//    we care about.  The 'right' way to deal with this is to update 
	//    Falcor to provide a more granular way of accessing profile data that
	//    isn't stringified.  For now, I'll just extract the data from 
	//    the string.  So, yeah, this is ugly.  And if Falcor changes the profiler,
	//    this will need updating.

	// Get our Falcor profiler string
	std::string profileMsg = Profiler::getEventsString();
    mTmpStr = profileMsg;

	// Grab all of the lines with profiling data
	int findPass = 0;
	int count = 1;

	size_t endOfLine = profileMsg.find('\n');
	while (endOfLine != std::string::npos && findPass < mActivePasses.size())
	{
		// Each RenderPass is profiled with the name "Pass_%d" where %d various from 0 to (#passes-1)
		char buf[128], passName[128];
		float cpuTime, gpuTime;
		sprintf_s(buf, "Pass_%d", findPass);

		// See if this line contains the time for the next pass (each line contains: <profileEventName> <cpuTime> <gpuTime>)
		sscanf( profileMsg.substr(0, endOfLine).c_str(), "%s %f %f ", passName, &cpuTime, &gpuTime );
		if (!strcmp(buf, passName))
		{
			mProfileGPUTimes[findPass++] = gpuTime;	
		}

		// Move onto the next line of the string.
		profileMsg = profileMsg.substr(endOfLine+1);
		endOfLine = profileMsg.find('\n');
	}
}


void RenderingPipeline::run(RenderingPipeline *pipe, SampleConfig &config)
{
	pipe->updatePipelineRequirementFlags();
	Sample::run(config, std::unique_ptr<Renderer>(pipe));
}