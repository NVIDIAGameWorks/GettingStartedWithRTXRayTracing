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
#include "ResourceManager.h"

/** Abstract base class for render passes.
*/
class RenderPass : public std::enable_shared_from_this<::RenderPass>
{
public:
    using SharedPtr = std::shared_ptr<::RenderPass>;
    using SharedConstPtr = std::shared_ptr<const ::RenderPass>;

    virtual ~RenderPass() = default;

protected:

	//
	// Interface for derived classes. See description of parameters for the corresponding functions, below, in the public interface.
	//

	virtual bool initialize(Falcor::RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) = 0;
	virtual void initScene(Falcor::RenderContext* pRenderContext, Falcor::Scene::SharedPtr pScene) {}
	virtual void resize(uint32_t width, uint32_t height) {}
	virtual void pipelineUpdated(ResourceManager::SharedPtr pResManager) { mpResManager = pResManager; }
	virtual bool processKeyEvent(const Falcor::KeyboardEvent& keyEvent) { return false; }
	virtual bool processMouseEvent(const Falcor::MouseEvent& mouseEvent) { return false; }
	virtual void renderGui(Falcor::Gui* pGui) {}
	virtual void execute(Falcor::RenderContext* pRenderContext) = 0;
	virtual void shutdown() {}
	virtual void stateRefreshed() {}
	virtual void activatePass() {}
	virtual void deactivatePass() {}

public:

	// Override these to describes aspects of the pass to the pipeline.  Setting these correctly is not vital, but
	//     may provide global options in the UI (e.g., displaying a 'Load Scene' button) that are useful
	virtual bool requiresScene()      { return false; }      // Will your pass require access to a scene?
	virtual bool loadDefaultScene()   { return false; }      // If set to true, load a default scene on initialization
	virtual bool usesRasterization()  { return false; }      // Will your pass rasterize a scene?
	virtual bool usesRayTracing()     { return false; }      // Will your pass ray trace a scene?
	virtual bool usesCompute()        { return false; }      // Will your pass use a compute pass?
	virtual bool appliesPostprocess() { return false; }      // Does your pass apply a postprocess?
	virtual bool usesEnvironmentMap() { return false; }      // Does your pass use an environment map?
	virtual bool hasAnimation()       { return true;  }      // Controls if "freeze animation" GUI is shown (should generally leave as true)


    //
    // Public interface. These functions call corresponding virtual protected interface functions.
    //

    /** Callback on application initialization.
        \param[in] context Provides the current context to initialize resources for your renderer.
		\param[in] resManager Manager for shared resources between our passes
        \return false if initialization failed, true otherwise.
    */
    bool onInitialize(Falcor::RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager);

    /** Callback on scene initialization.
        \param[in] context Provides the current context to initialize resources for your renderer.
        \param[in] scene Provides the newly loaded scene.
    */
    void onInitScene(Falcor::RenderContext* pRenderContext, Falcor::Scene::SharedPtr pScene) { initScene(pRenderContext, pScene); }

	/** Callback for when the pipeline state changes.
	    \param[in] resourceManager Provides the current resource manager, which has some state changed since the last call.
	*/
	void onPipelineUpdate(ResourceManager::SharedPtr pResManager) { pipelineUpdated(pResManager); }
	
	/** Called whenever any RenderPass in the current pipeline sets its mRefreshFlag
	*/
	void onStateRefresh(void) { stateRefreshed(); }

    /** Callback when the image/resources need to be resized. Called once at startup and when the window is resized.
        \param[in] width The new width of your window.
        \param[in] height The new height of your window.
    */
    void onResize(uint32_t width, uint32_t height) { resize(width, height); }

    /** Callback executed when processing a key event.
        \param[in] keyEvent A Falcor structure containing details about the event
        \return true if we process the key event, false if someone else should
    */
    bool onKeyEvent(const Falcor::KeyboardEvent& keyEvent) { return processKeyEvent(keyEvent); }

    /** Callback executed when processing a mouse event.
        \param[in] mouseEvent A Falcor structure containing details about the event
        \return true if we process the mouse event, false if someone else should
    */
    bool onMouseEvent(const Falcor::MouseEvent& mouseEvent) { return processMouseEvent(mouseEvent); }

    /** Callback on GUI render.
        \param[in] pGui GUI instance to render UI elements with.
    */
    void onRenderGui(Falcor::Gui* pGui);

    /** Callback for executing render pass.
        \param[in] context Provides the current context to initialize resources for your renderer
    */
    void onExecute(Falcor::RenderContext* pRenderContext);

    /** Callback executed when closing the application.
    */
    void onShutdown();

	/** Called when this pass is activated via the UI (to add it to your pipeline)
	*/
	void onPassActivation() { activatePass(); }

	/** Called when this pass is deactivated via the UI (removed from your pipeline)
	*/
	void onPassDeactivation() { deactivatePass(); }

    //
    // Public utility functions. These configure the render pass name and UI window.
    //

    /** Sets the name of the render pass.
    */
    void setName(const std::string& name) { mName = name; }

    /** Returns the name of the render pass.
    */
    std::string getName() const { return mName; }

    /** Sets the name of the GUI window/group.
    */
    void setGuiName(const std::string& guiName) { mGuiName = guiName; }

    /** Returns the GUI window/group name.
    */
    std::string getGuiName() const { return mGuiName; }

    /** Returns true if this render pass should get its own UI window.
    */
    bool useGuiWindow() const { return true; }

    /** Set position in pixels of the UI window in the application window's client area.
    */
    void setGuiPosition(const glm::ivec2& guiPos) { mGuiPosition = guiPos; }

    /** Get position in pixels of the UI window in the application window's client area.
    */
    glm::ivec2 getGuiPosition() const { return mGuiPosition; }

    /** Set size in pixels of the UI window in the application window's client area.
    */
    void setGuiSize(const glm::ivec2& guiSize) { mGuiSize = guiSize; }

    /** Get size in pixels of the UI window in the application window's client area.
    */
    glm::ivec2 getGuiSize() const { return mGuiSize; }

    /** Returns true if render pass is successfully initialized.
    */
    bool isInitialized() const { return mIsInitialized; }

    /** Returns true if refresh flag is set. The flag is automatically reset after execute() is called.
    */
    bool isRefreshFlagSet() const { return mRefreshFlag; }

    /** Returns true if rebind flag is set. The flag is manually reset by calling resetRebindFlag().
    */
    bool isRebindFlagSet() const { return mRebindFlag; }

    /** Resets the rebind flag.
    */
    void resetRebindFlag() { mRebindFlag = false; }

protected:
    /** Constructor.
        \param[in] name The name of the render pass.
        \param[in] guiName The name of the GUI group window.
    */
	RenderPass(const std::string name = "<Unknown render pass>",
               const std::string guiName = "<Unknown gui group>") : mName(name), mGuiName(guiName) {}

	RenderPass(const RenderPass&) = delete;
	RenderPass& operator=(const RenderPass &) = delete;

    /** Set the refresh flag.
    */
    void setRefreshFlag() { mRefreshFlag = true; }

    /** Set the rebind flag.
    */
    void setRebindFlag() { mRebindFlag = true; }

private:
    // Internal state
    std::string mName;                          ///< Name of the render pass.
    std::string mGuiName;                       ///< Name of the GUI group/window for this render pass.
    glm::ivec2 mGuiPosition = { -270, 30 };     ///< Position in pixels of the UI window in the application window's client area.
    glm::ivec2 mGuiSize = { 250, 160 };         ///< Size in pixels of the UI window in the application window's client area.

    bool mIsInitialized = false;                ///< Set to true upon successful intialization.
    bool mRefreshFlag = true;                   ///< User flag that is automatically reset after execute().
    bool mRebindFlag = true;                    ///< User flag that is manually reset by calling resetRebindFlag().

protected:
    ResourceManager::SharedPtr mpResManager;    ///< All passes will need to talk to the resource manager, so will need to stash a copy
};
