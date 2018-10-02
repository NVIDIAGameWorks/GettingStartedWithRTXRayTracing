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
#include <vector>
#include <map>

using namespace Falcor;

class ResourceManager : public std::enable_shared_from_this<ResourceManager>
{
public:
    using SharedPtr = std::shared_ptr<ResourceManager>;
    using SharedConstPtr = std::shared_ptr<const ResourceManager>;
	
	static const std::string kOutputChannel; 
	static const std::string kEnvironmentMap;

	// Public ctors and dtors
	static SharedPtr create(uint32_t width, uint32_t height, SampleCallbacks *callbacks);
	virtual ~ResourceManager() = default;

	// Set a default set of resource flags for managed textures.  (Default allows SRV, UAV, and RTV access, though this may be somewhat less performant)
	static const Resource::BindFlags kDefaultFlags     = Resource::BindFlags(uint32_t(Resource::BindFlags::ShaderResource) | uint32_t(Resource::BindFlags::UnorderedAccess) | uint32_t(Resource::BindFlags::RenderTarget) );
	static const Resource::BindFlags kDepthBufferFlags = Resource::BindFlags(uint32_t(Resource::BindFlags::ShaderResource) | uint32_t(Resource::BindFlags::DepthStencil));

	// Instead of trying to manually share resources, you can request the ResourceManager to create and manage the texture for you.
	//    -> Return: an index to request access to managed textures (faster than asking for them by name)
	//    -> If request conflicts with another pass (i.e., asked for different formats/size), returns -1. 
	//    -> If width/height parameters are -1, creates a full-screen resource that is automatically rezied when the window is resized
	//    -> If width/height parameters are specified, texture needs to be manually resized.
	int32_t requestTextureResource(const std::string &channelName, ResourceFormat channelFormat = ResourceFormat::RGBA32Float, Resource::BindFlags usageFlags = kDefaultFlags, int32_t channelWidth=-1, int32_t channelHeight=-1);

	// The same as above, but requests multiple textures with the same format
	void requestTextureResources(const std::vector<std::string> &channelNames, ResourceFormat channelFormat = ResourceFormat::RGBA32Float, Resource::BindFlags usageFlags = kDefaultFlags, int32_t channelWidth = -1, int32_t channelHeight = -1);

	// If a pass has created a resource other passes may want to share, it can request it to be managed, and pass it to the resource manager
	int32_t manageTextureResource(const std::string &channelName, Texture::SharedPtr sharedTex);

	// Resize an existing texture resource. 
	//     -> If newWidth and newHeight are specified, the resource will be resized and will stay that size until manually resized
	//     -> If newWidth or newHeight are -1, the resource will be sized to the current window and will be resized when the window size changes.
	void updateTextureSize(const std::string &channelName, int32_t newWidth = -1, int32_t newHeight = -1);
	void updateTextureSize(int32_t channelIdx, int32_t newWidth = -1, int32_t newHeight = -1);

	// Get a pointer to the texture with the specified channel name or channel index.  Returns a nullptr if channel does not exist
	Texture::SharedPtr getTexture(const std::string &channelName);
	Texture::SharedPtr getTexture(int32_t channelIdx);

	// Get a pointer to requested texture, but before returning, clear the channel
	Texture::SharedPtr getClearedTexture(const std::string &channelName, vec4 &clearColor);
	Texture::SharedPtr getClearedTexture(int32_t channelIdx, vec4 &clearColor);

	// If you have a texture, you can clear it here
	void clearTexture(Texture::SharedPtr &tex, const vec4 &clearColor);

	// Returns the name of the texture with the specified index
	std::string getTextureName(int32_t channelIdx);

	// Returns the channel index of the channel with the specified name (returns -1 if channel name does not exist)
	int32_t getTextureIndex(const std::string &channelName) const;

	// Return the maximum number of channels we might have (some may be invalid)
	uint32_t getTextureCount(void) const { return uint32_t(mTextures.size()); }

	// Will update the stored environment map to the specified file.  Returns <true> if
	//     the resource manager was able to load the specified file.  If the load fails,
	//     the prior environment map is still used.
	bool updateEnvironmentMap(const std::string &filename);

	// Get details about the internally managed environment map
	std::string  getEnvironmentMapName(void) const { return mEnvMapFilename; }
	Texture::SharedPtr getEnvironmentMap() { return getTexture( kEnvironmentMap );  }
	uvec2 getEnvironmentMapSize() const;

	// Creates a framebuffer from a set of resources managed by the ResourceManager.  
	//    -> Note:  This FBO remains valid until haveResourcesChanged() is true, at which point the user needs to recreate it
	//    -> Color buffers are attached based on their location in the vector.  Invalid indicies (i.e., -1) can be inserted 
	//       in the list to leave an attachment point unbound.
	//    -> Returns nullptr if (a) any resource in the color list is a depth/stencil texture, (b) the depth/stencil index 
	//       is not a depth/stencil texture, or (c) there is not at least one resource bound.
	//    -> If you try to bind too many color buffers (currently > 8), additional buffers are ignored silently.
	//    -> Texture sizes not checked for consistency.
	Fbo::SharedPtr createManagedFbo(const std::vector<int32_t> &colorBufIndicies, int32_t depthStencilBufIdx = -1);
	Fbo::SharedPtr createManagedFbo(const std::vector<std::string> &colorBufIndicies, const std::string &depthStencilBufIdx = "");

	// A set of wrappers over Falcor FBO creation to simplify/eliminate some boilerplate code.  
	//      NOTE: These FBOs are not managed by the ResourceManager.
	//
	// Create a FBO with a single color buffer with the specified color format and (optionally) a depth/stencil buffer
	static Fbo::SharedPtr createFbo(uint32_t width, uint32_t height, ResourceFormat colorFormat = ResourceFormat::RGBA32Float, bool hasDepthStencil = false);

	// Create a FBO with a variable number of color buffers (with individually specified buffer formats) and (optionally) a depth/stencil buffer
	static Fbo::SharedPtr createFbo(uint32_t width, uint32_t height, std::vector<ResourceFormat> colorFormats, bool hasDepthStencil = false);

	// Get the default system FBO.  You should not write to this, but you can use it if you need to bind an FBO.
	Fbo::SharedPtr getDefaultFbo() { return mpAppCallbacks->getCurrentFbo(); }

	// Did someone specify the default scene to load?
	std::string getDefaultSceneName() { return mDefaultSceneName; }    // Return the default scene name
	void setDefaultSceneName(const std::string &sceneFilename);        // Set the default scene name
	bool userSetDefaultScene() const { return mUserSetDefaultScene; }  // Return 'true' if setDefaultSceneName() has been called

    // Resize the buffers to the new size, if it is different. 
    void resize(uint32_t width, uint32_t height);

	// Initializes internal ResourceManager resources.
	void initializeResources();
	bool isInitialized(void) const { return mIsInitialized; }

    // Get the resolution of fullscreen channels.
    uint32_t getWidth() const      { return mWidth; }
    uint32_t getHeight() const     { return mHeight; }
	uvec2    getScreenSize() const { return uvec2(mWidth, mHeight); }

	// If resources have changed since last frame (and previous resource pointers may be invalid), this will return true
	bool haveResourcesChanged() const { return mUpdatedFlag; }

	// The owner of the resource manager can reset the dirty flag after passes have have been notified
	void resetDirtyFlag() { mUpdatedFlag = false; }

	// We probably want to share some common ray tracing state
	float getMinTDist() const        { return mMinT; }
	void  setMinTDist(float newMinT) { mMinT = newMinT; }

protected:
	ResourceManager(uint32_t width, uint32_t height, SampleCallbacks *callbacks) : mWidth(width), mHeight(height), mpAppCallbacks(callbacks) {}

    // Various internal state
    uint32_t mWidth = 0;    
    uint32_t mHeight = 0;       
	bool     mIsInitialized = false;
	bool     mUpdatedFlag = true;
	float    mMinT = 1.0e-4f;

	// If using the resource manager to manage an environment map, its filename is here.
	std::string mEnvMapFilename = "";

	// Can specify the default scene to load
	std::string mDefaultSceneName = "Media/Arcade/Arcade.fscene";
	bool        mUserSetDefaultScene = false;    // If the developer changes the default scene, assume they want it loaded.

	// Falcor's callbacks structure to access basic resources of the application
	SampleCallbacks *mpAppCallbacks;

    // The internal texture resources.  These could be combined into an AoS rather than a SoA, but I was lazy.  Does it matter?
    std::vector<Texture::SharedPtr>   mTextures;         ///< The texture resources managed by this class
	std::vector<std::string>          mTextureNames;     ///< std::string-based names for the textures
	std::vector<glm::ivec2>           mTextureSizes;     ///< Stored separately from internal texture data so we can distinguish between fixed & fullscreen textures
	std::vector<Resource::BindFlags>  mTextureFlags;     ///< Expected usage flags
	std::vector<ResourceFormat>       mTextureFormat;    ///< Expected texture format

private:
	// These are not meant to be exposed outside the class and may not have suitable error checking non-private use.
	bool hasBindFlag(int32_t index, Resource::BindFlags flag);

};
