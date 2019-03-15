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
#include "RenderPass.h"
#include "ResourceManager.h"

class RenderingPipeline : public Renderer, inherit_shared_from_this<Renderer, RenderingPipeline>
{
public:
    using SharedPtr = std::shared_ptr<RenderingPipeline>;
 
	// Our publically-visible constructors 
	RenderingPipeline();
	virtual ~RenderingPipeline() = default;

	/** Lets derived classes setup a pipeline. Function can be called before or after the renderer has been initialized.
	\param[in] passNum  The pass to change.  Call ignored if passNum > mActivePasses.size().  (New pass inserted if passNum == mActivePasses.size())
	\param[in] pTargetPass  Which pass should we use?  If pTargetPass is not in mAvailPasses, it is added.
	\param[in] canAddPassAfter  Should the user be able to insert a new pass immediately after this one?
	\param[in] canRemovePass  Should the user be able to remove this pass?
	*/
	void setPass(uint32_t passNum, ::RenderPass::SharedPtr pTargetPass, bool canAddPassAfter = false, bool canRemovePass = false);

	/** As with setPass(), but allows multiple RenderPass inputs that can be swapped between
	*/
	void setPassOptions(uint32_t passNum, std::vector< ::RenderPass::SharedPtr > pPassList);


	/** Add a pass to the list of those selectable to be used in this renderer.  Should occur before Sample::run()!
	    \param[in] pNewPass Shared pointer to a render pass to be selectable by the yser.
	    \return An integer identifier specifying location in the internal list of render passes.
	*/
	uint32_t addPass(::RenderPass::SharedPtr pNewPass);

	/** To start running the application with this rendering pipeline, call this method
	*/
	static void run(RenderingPipeline *pipe, SampleConfig &config);

	// Overloaded methods from MyRenderer
	virtual void onLoad(SampleCallbacks* pSample, const RenderContext::SharedPtr &pRenderContext) override;
	virtual void onFrameRender(SampleCallbacks* pSample, const RenderContext::SharedPtr &pRenderContext, const Fbo::SharedPtr &pTargetFbo) override;
	virtual void onShutdown(SampleCallbacks* pSample) override;
	virtual void onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height) override;
	virtual void onDataReload(SampleCallbacks* pSample) override {}
	virtual bool onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent) override;
	virtual bool onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent) override;
	virtual void onGuiRender(SampleCallbacks* pSample, Gui* pGui) override;
	virtual void onDroppedFile(SampleCallbacks* pSample, const std::string& filename) override {}
    
protected:
	/** When a new scene is loaded, this gets called to let any passes in this pipeline know there's a new scene.
	*/
	void onInitNewScene(RenderContext* pRenderContext, Scene::SharedPtr pScene);

    /** Returns an ordered list of currently active passes without gaps.
    */
    void getActivePasses(std::vector<::RenderPass::SharedPtr>& activePasses) const;

    /** Returns the last known swap chain size.
    */
    glm::uvec2 getSwapChainSize() const { return mLastKnownSize; }

	/** Allows the developer to add a description to be displayed about the pass list
	*/
	void addPipeInstructions(const std::string &str);

private:

	// On the first execution of onFrameRender(), we're calling this
	void onFirstRun(SampleCallbacks* pSample);

	// Want to remove a pass from the list?  
	void removePassFromPipeline(uint32_t passNum);

	// Add a NULL pass between existing passes in the pipeline?  (The user can then change the pass via the UI)
	void insertPassIntoPipeline(uint32_t afterPass);

	// Do what needs to be done to change the render pass executed in order <passNum> to use the pass in <pNewPass>.
	void changePass(uint32_t passNum, ::RenderPass::SharedPtr pNewPass);

	// Creates a UI dropdown for inserting/changing the pass at order <passNum> in the list of active passes.
	void createDropdownGuiForPass(uint32_t passNum, Gui::DropdownList& outputList);
	void createDefaultDropdownGuiForPass(uint32_t passNum, Gui::DropdownList& outputList);

	// Checks if <pCheckPass> is valid to insert at order <passNum> in the list of active passes.
	bool isPassValid(::RenderPass::SharedPtr pCheckPass, uint32_t passNum);

	// Check if any of the active passes have requested a pipeline change; also resets pass rebind flags.
	bool anyRequestedPipelineChanges(void);

	// Check if any passes have set a refresh flag (i.e., settings changed; temporal history should be invalidated)
	bool havePassesSetRefreshFlag(void);

	// Update the mPipeRequires* member variables
	void updatePipelineRequirementFlags(void);

	// Extract profiling data
	void extractProfilingData(void);

	enum UIOptions { CanRemove = 0x1u, CanAddAfter = 0x2u };

	// Internal state
	std::vector<::RenderPass::SharedPtr> mAvailPasses;        ///< List of all passes available to be selected. List is unordered (or order unimportant).
	std::vector<::RenderPass::SharedPtr> mActivePasses;       ///< Ordered list of currently active passes
	std::vector< Gui::DropdownList > mPassSelectors;        ///< Ordered list of GUI selectors used to change currently active passes
	std::vector< uint32_t > mPassId;                        ///< Stores UI variables for currently selected passes.
	std::vector< bool > mEnablePassGui;                     ///< Stores whether the UI window for each pass is enabled
	std::vector< int32_t > mEnableAddRemove;                ///< Stores whether the UI allows adding after this pass or removing this pass
	uvec2 mLastKnownSize = uvec2(0);                        ///< Last known size sent to onResizeSwapChain().
	bool mPipelineChanged = true;                           ///< A flag to keep track of pipeline changes
	bool mIsInitialized = false;
	bool mDoProfiling = false;
    bool mProfileToggle = false;
	bool mFirstFrame = true;
	bool mUseSceneCameraPath = false;
	bool mFreezeTime = true;
	bool mGlobalPipeRefresh = false;
	ResourceManager::SharedPtr mpResourceManager;
	int32_t mOutputBufferIndex = 0;
	Scene::SharedPtr mpScene = nullptr;                     ///< Stash a copy of our scene
	CameraController::SharedPtr mpCameraControl;
	GraphicsState::SharedPtr mpDefaultGfxState;
	std::vector< std::string > mPipeDescription;            ///< Can store a description of the pipeline for display in the UI
	std::vector< HashedString > mProfileNames;
	std::vector< double > mProfileGPUTimes;
    std::vector< double > mProfileLastGPUTimes;

	// Are we storing an environment map?
	Gui::DropdownList mEnvMapSelector;

	// These are Chris-specific things...  He likes particular HDR lightprobes and inserts default menu options to load them
	bool mHasMonValley = false; 
	std::string mMonValleyFilename;

	// Whenever the pipeline changes, we query to see if any passes have the following properties
	bool mPipeRequiresScene      = false;
	bool mPipeRequiresRaster     = false;
	bool mPipeRequiresRayTracing = false;
	bool mPipeAppliesPostprocess = false;
	bool mPipeUsesCompute        = false;
	bool mPipeUsesEnvMap         = false;
	bool mPipeNeedsDefaultScene  = false;
	bool mPipeHasAnimation       = true;

	// Helpers to clarify code querying if a pass can be removed (or another can be added after it)
	bool canRemovePass(uint32_t passNum);
	bool canAddPassAfter(uint32_t passNum);

	// A dropdown allowing us to select which minimum t distance to use for ray tracing
	Gui::DropdownList mMinTDropdown = { { 0, "0.1" }, { 1, "0.01" }, { 2, "0.001" },{ 3, "1e-4" }, { 4, "1e-5" }, { 5, "1e-6" }, { 6, "1e-7" }, {7, "0"} };
	float             mMinTArray[8] = { 0.1f, 0.01f, 0.001f, 1e-4f, 1e-5f, 1e-6f, 1e-7f, 0.0f };
	uint32_t          mMinTSelection = 3;

    std::string mTmpStr = "";
};
