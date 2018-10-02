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

#include "ResourceManager.h"

// The fixed resource name of our output channel
const std::string ResourceManager::kOutputChannel  = "PipelineOutput";
const std::string ResourceManager::kEnvironmentMap = "EnvironmentMap";

ResourceManager::SharedPtr ResourceManager::create(uint32_t width, uint32_t height, SampleCallbacks *callbacks)
{
	return SharedPtr(new ResourceManager(width, height, callbacks));
}

void ResourceManager::resize(uint32_t width, uint32_t height)
{
	// Don't spend time resizing resources if we didn't change resolutions!
	if (width == mWidth && height == mHeight) return;

	// Stash our new screensize
	mWidth = width;
	mHeight = height;

	// We can't really do anything with resources when the screen size is 0
	if (mWidth <= 0 || mHeight <= 0) return;

	// If we've gotten this far without initializing our resources, do that now.
	if (!mIsInitialized)
		initializeResources();

	// Resize our resources that dynamically resize.
	for (int32_t i = 0; i < int32_t(mTextures.size()); i++)
	{
		// Only resize textures that are defined to be screensize
		if (mTextureSizes[i] != ivec2(-1, -1)) continue;

		// Recreate our texture with the new size
		mTextures[i] = Texture::create2D(mWidth, mHeight, mTextureFormat[i], 1u, 1u, nullptr, mTextureFlags[i]);
	}

	mUpdatedFlag = true;
}

void ResourceManager::initializeResources()
{
	// Create all textures that have not been allocated otherwise.
	for (int32_t i = 0; i < int32_t(mTextures.size()); i++)
	{
		// Either use explicitly specified texture sizes, or if no size specified texture is assumed to be full-screen
		uint32_t texWidth = mTextureSizes[i].x <= 0 ? mWidth : mTextureSizes[i].x;
		uint32_t texHeight = mTextureSizes[i].y <= 0 ? mHeight : mTextureSizes[i].y;

		// Create the resource (unless it already exists, because a pass created it and passed it in to be managed)
		if (!mTextures[i])
			mTextures[i] = Texture::create2D(texWidth, texHeight, mTextureFormat[i], 1u, 1u, nullptr, mTextureFlags[i]);
	}

	mIsInitialized = true;
	mUpdatedFlag = true;
}

bool ResourceManager::updateEnvironmentMap(const std::string &filename)
{
	// Pass in a NULL file?  Change to our default texture
	if (filename == "")
	{
		Texture::SharedPtr tmpEnv = Texture::create2D(128, 128, ResourceFormat::RGBA32Float, 1u, 1u, nullptr, ResourceManager::kDefaultFlags);
		mpAppCallbacks->getRenderContext()->clearUAV(tmpEnv->getUAV().get(), vec4(0.5f, 0.5f, 0.8f, 1.0f));
		manageTextureResource(ResourceManager::kEnvironmentMap, tmpEnv);
		mUpdatedFlag = true;
		return true;
	}
	else if (filename == "Black")
	{
		Texture::SharedPtr tmpEnv = Texture::create2D(128, 128, ResourceFormat::RGBA32Float, 1u, 1u, nullptr, ResourceManager::kDefaultFlags);
		mpAppCallbacks->getRenderContext()->clearUAV(tmpEnv->getUAV().get(), vec4(0.0f, 0.0f, 0.0f, 1.0f));
		manageTextureResource(ResourceManager::kEnvironmentMap, tmpEnv);
		mUpdatedFlag = true;
		return true;
	}
	else if (filename == "Carolina sky blue")
	{
		Texture::SharedPtr tmpEnv = Texture::create2D(128, 128, ResourceFormat::RGBA32Float, 1u, 1u, nullptr, ResourceManager::kDefaultFlags);
		mpAppCallbacks->getRenderContext()->clearUAV(tmpEnv->getUAV().get(), vec4(0.078f, 0.361f, 0.753f, 1.0f));
		manageTextureResource(ResourceManager::kEnvironmentMap, tmpEnv);
		mUpdatedFlag = true;
		return true;
	}
	else
	{
		// Non null file?  Try to load it.
		Texture::SharedPtr envMap = createTextureFromFile(filename, false, false);
		if (envMap)
		{
			// Success.  Update the filename we loaded and remember to manage this texture we just loaded.
			size_t found = filename.find_last_of("/\\");
			mEnvMapFilename = filename.substr(found + 1).c_str();
			manageTextureResource(ResourceManager::kEnvironmentMap, envMap);
			mUpdatedFlag = true;
			return true;
		}
	}
	return false;
}

uvec2 ResourceManager::getEnvironmentMapSize() const
{
	int32_t existingIndex = getTextureIndex(ResourceManager::kEnvironmentMap);
	if (existingIndex < 0) return uvec2(0, 0);

	return uvec2( mTextureSizes[existingIndex] );
}

int32_t ResourceManager::manageTextureResource(const std::string &channelName, Texture::SharedPtr sharedTex)
{
	// See if we've already defined this channel
	int32_t existingIndex = getTextureIndex(channelName);

	// No existing resource with that name.  Create one.
	if (existingIndex <= 0)
	{
		existingIndex = int32_t(mTextures.size());
		mTextures.push_back(nullptr);
		mTextureSizes.push_back(ivec2(-1, -1));
		mTextureNames.push_back(channelName);
		mTextureFlags.push_back(kDefaultFlags);
		mTextureFormat.push_back(sharedTex->getFormat());
	}

	// Override requested resolution and format based on the incoming texture
	mTextureFormat[existingIndex] = sharedTex->getFormat();
	mTextureSizes[existingIndex] = ivec2(sharedTex->getWidth(), sharedTex->getHeight());

	// Store our texture pointer
	mTextures[existingIndex] = sharedTex;

	// Since we passed in an existing texture, it has the usage flags it was created with. 
	mTextureFlags[existingIndex] = kDefaultFlags;

	// Make sure to note that our resources have updated
	mUpdatedFlag = true;
	return existingIndex;
}

int32_t ResourceManager::getTextureIndex(const std::string &channelName) const
{
	auto item = std::find(mTextureNames.begin(), mTextureNames.end(), channelName);
	int32_t channelIdx = int32_t(item - mTextureNames.begin());
	return (channelIdx >= mTextureNames.size()) ? -1 : channelIdx;
}

std::string ResourceManager::getTextureName(int32_t channelIdx)
{
	if (channelIdx < 0 || channelIdx >= mTextureNames.size()) 
		return std::string("< Invalid Channel >");
	return mTextureNames[channelIdx];
}

Texture::SharedPtr ResourceManager::getTexture(int32_t channelIdx)
{
	if (channelIdx < 0 || channelIdx >= mTextures.size())
		return nullptr;
	return mTextures[channelIdx];
}

Texture::SharedPtr ResourceManager::getTexture(const std::string &channelName)
{
	return getTexture(getTextureIndex(channelName));
}

Texture::SharedPtr ResourceManager::getClearedTexture(const std::string &channelName, vec4 &clearColor)
{
	Texture::SharedPtr channel = getTexture(channelName);
	if (!channel) return nullptr;

	mpAppCallbacks->getRenderContext()->clearUAV(channel->getUAV().get(), clearColor);
	return channel;
}

Texture::SharedPtr ResourceManager::getClearedTexture(int32_t channelIdx, vec4 &clearColor)
{
	Texture::SharedPtr channel = getTexture(channelIdx);
	if (!channel) return nullptr;

	mpAppCallbacks->getRenderContext()->clearUAV(channel->getUAV().get(), clearColor);
	return channel;
}

void ResourceManager::clearTexture(Texture::SharedPtr &tex, const vec4 &clearColor)
{
	// Figure out what type of texture this is
	Resource::BindFlags flags = tex->getBindFlags();

	// Clear as appropriate.  If a depth texture, clear the depth with the red channel of the clear color
	if ((flags & Resource::BindFlags::RenderTarget) == Resource::BindFlags::RenderTarget)
		mpAppCallbacks->getRenderContext()->clearRtv(tex->getRTV().get(), clearColor);
	else if ((flags & Resource::BindFlags::UnorderedAccess) == Resource::BindFlags::UnorderedAccess)
		mpAppCallbacks->getRenderContext()->clearUAV(tex->getUAV().get(), clearColor);
	else if ((flags & Resource::BindFlags::DepthStencil) == Resource::BindFlags::DepthStencil)
		mpAppCallbacks->getRenderContext()->clearDsv(tex->getDSV().get(), clearColor.r, 0);
}

int32_t ResourceManager::requestTextureResource(const std::string &channelName, 
	ResourceFormat channelFormat, Resource::BindFlags usageFlags, int32_t channelWidth, int32_t channelHeight)
{
	// See if we've already defined this texture
	int32_t existingIndex = getTextureIndex(channelName);

	if (existingIndex >= 0)
	{
		// Check for mismatches that might mean requestors for this buffer have conflicting needs
		if (channelFormat != mTextureFormat[existingIndex]) return -1;
		if (mTextureSizes[existingIndex] != ivec2(channelWidth, channelHeight)) return -1;

		// If we've asked for more usage types than anyone else, update the resource
		mTextureFlags[existingIndex] |= usageFlags;

		// Looks like a match!  Return an index for the exisitng resource
		return existingIndex;
	}

	// No existing resource with that name.  Create one.
	existingIndex = int32_t(mTextures.size());
	mTextures.push_back(nullptr);    // We'll actually create the resource in initializeResources()
	mTextureSizes.push_back(ivec2(channelWidth, channelHeight));
	mTextureNames.push_back(channelName);
	mTextureFlags.push_back(usageFlags);
	mTextureFormat.push_back(channelFormat);

	// While we haven't changed existing resources, it's probably good to notify users that resources available have changed
	mUpdatedFlag = true;

	return existingIndex;
}

void ResourceManager::requestTextureResources(const std::vector<std::string> &channelNames,
	ResourceFormat channelFormat, Resource::BindFlags usageFlags, int32_t channelWidth, int32_t channelHeight)
{
	// Not really efficient.  Simply a wrapper around multiple requestTextureResource() calls.
	for (uint32_t i = 0; i < channelNames.size(); i++)
	{
		requestTextureResource(channelNames[i], channelFormat, usageFlags, channelWidth, channelHeight);
	}
}

void ResourceManager::setDefaultSceneName(const std::string &sceneFilename) 
{ 
	mDefaultSceneName = sceneFilename; 
	mUserSetDefaultScene = true;
}


Fbo::SharedPtr ResourceManager::createManagedFbo(const std::vector<int32_t> &colorBufIndicies, int32_t depthStencilBufIdx)
{
	bool hasDepthStencilBuf = false;
	bool hasColorBuf = false;
	Fbo::SharedPtr pFbo = Fbo::create();

	// Is the depth-stencil index a valid texture?
	if (depthStencilBufIdx >= 0 && depthStencilBufIdx < int32_t(mTextures.size()))
	{
		// Was that texture set up with a DepthStencil format and binding flag?
		if (isDepthStencilFormat(mTextureFormat[depthStencilBufIdx]) && 
			hasBindFlag(depthStencilBufIdx, Resource::BindFlags::DepthStencil))
		{
			pFbo->attachDepthStencilTarget(mTextures[depthStencilBufIdx]);
			hasDepthStencilBuf = true;
		}
	}

	// Bind color textures
	for (int32_t i = 0; i < int32_t(colorBufIndicies.size()); i++)
	{
		// Ignore and bind no texture at slot i if:
		if (colorBufIndicies[i] < 0 || colorBufIndicies[i] >= int32_t(mTextures.size())) continue;  // it's an invalid index; no such resource exists
		if (isDepthStencilFormat(mTextureFormat[colorBufIndicies[i]])) continue;                    // it's a depth/stencil buffer
		if (!hasBindFlag(colorBufIndicies[i], Resource::BindFlags::RenderTarget)) continue;         // it can't be bound as a render target
		if (i >= int32_t(Fbo::getMaxColorTargetCount())) continue;                                  // We've exceeded the number of allowable color targets

		pFbo->attachColorTarget(mTextures[colorBufIndicies[i]], i);
		hasColorBuf = true;
	}

	// We need to have either a depth/stencil or color buffer bound. 
	if (!hasDepthStencilBuf && !hasColorBuf) 
		return nullptr;

	// We have a valid FBO!  Return it.
	return pFbo;
}

Fbo::SharedPtr ResourceManager::createManagedFbo(const std::vector<std::string> &colorBufIndicies, const std::string &depthStencilBufIdx)
{
	// Convert the strings into buffer indicies, then call the createFbo() method that takes indicies
	std::vector<int32_t> colorIndicies;
	int32_t depthIndex = getTextureIndex(depthStencilBufIdx);
	for (int32_t i = 0; i < int32_t(colorBufIndicies.size()); i++)
	{
		colorIndicies.push_back(getTextureIndex(colorBufIndicies[i]));
	}

	return createManagedFbo(colorIndicies, depthIndex);
}

bool ResourceManager::hasBindFlag(int32_t index, Resource::BindFlags flag)
{
	return ((mTextureFlags[index] & flag) == flag);
}

void ResourceManager::updateTextureSize(const std::string &channelName, int32_t newWidth, int32_t newHeight)
{
	updateTextureSize(getTextureIndex(channelName), newWidth, newHeight);
}

void ResourceManager::updateTextureSize(int32_t channelIdx, int32_t newWidth, int32_t newHeight)
{
	if (channelIdx < 0 || channelIdx >= int32_t(mTextures.size()))
		return;

	// If either the dimensions < 0, treat this as a window-sized resource
	ivec2 newSize = ivec2(newWidth, newHeight);
	if (newWidth < 0 || newHeight < 0)
		newSize = ivec2(-1, -1);

	// If we haven't changed sizes, there's no reason to deallocate and reallocate the texture
	if (mTextureSizes[channelIdx] == newSize) return;

	// Update the channel
	mTextures[channelIdx] = Texture::create2D(newSize.x, newSize.y, mTextureFormat[channelIdx], 1u, Texture::kMaxPossible, nullptr, mTextureFlags[channelIdx]);
	mTextureSizes[channelIdx] = newSize;
	mUpdatedFlag = true;
}

Fbo::SharedPtr ResourceManager::createFbo(uint32_t width, uint32_t height, ResourceFormat colorFormat, bool hasDepthStencil)
{
	Fbo::Desc desc;

	// Create the color buffer; allow the texture to be used as a UAV.
	desc.setColorTarget(0, colorFormat, true);  

	// (Optionally) create our depth/stencil buffer
	if (hasDepthStencil)
		desc.setDepthStencilTarget(ResourceFormat::D24UnormS8); 

	return FboHelper::create2D(width, height, desc);
}

Fbo::SharedPtr ResourceManager::createFbo(uint32_t width, uint32_t height, std::vector<ResourceFormat> colorFormats, bool hasDepthStencil)
{
	Fbo::Desc desc;

	// Ensure we don't try to create an FBO with too many color buffers.  If the user requested this, we fail semi-silently.
	uint32_t loopMax = glm::min( uint32_t(colorFormats.size()), Fbo::getMaxColorTargetCount() );

	// Create our color buffers; allow them to be used as a UAV.
	for (uint32_t i = 0; i < loopMax; i++)
		desc.setColorTarget(i, colorFormats[i], true);  

	// (Optionally) create our depth/stencil buffer
	if (hasDepthStencil)
		desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);

	return FboHelper::create2D(width, height, desc);
}
