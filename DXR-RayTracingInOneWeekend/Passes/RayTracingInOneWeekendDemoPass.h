// This render pass does the heavy lifting for the "Ray Tracing In One Weekend" DXR sample.
//     If you understand this file and the corresponding .cpp file, you should understand the demo.

#pragma once
#include "../SharedUtils/RenderPass.h"   // The base class for all render passes in our app
#include "../SharedUtils/RayLaunch.h"    // The simple wrapper layer around DXR launches
#include <random>

class RayTracingInOneWeekendDemo : public RenderPass, inherit_shared_from_this<RenderPass, RayTracingInOneWeekendDemo>
{
public:
	// For consistency with other classes in Falcor / our tutorial code
    using SharedPtr = std::shared_ptr<RayTracingInOneWeekendDemo>;
    using SharedConstPtr = std::shared_ptr<const RayTracingInOneWeekendDemo>;

	// Constructors and destructors of various kinds
    static SharedPtr create() { return std::make_shared<RayTracingInOneWeekendDemo>(); }
	RayTracingInOneWeekendDemo() : RenderPass("Ray Tracing In One Weekend", "Ray Tracing In One Weekend Options") {}
	virtual ~RayTracingInOneWeekendDemo() = default;

protected:
    // Implementation of RenderPass interface (these are the callbacks that get called at important times)
    bool initialize(RenderContext::SharedPtr pRenderContext, ResourceManager::SharedPtr pResManager) override;  // Runs once on program load
    void execute(RenderContext::SharedPtr pRenderContext) override;                                             // Runs once per frame on redisplay
	void renderGui(Gui* pGui) override;                                                                         // Runs once per frame if displaying GUI
	bool processMouseEvent(const MouseEvent& mouseEvent) override;                                              // Runs when mouse events occur
	bool processKeyEvent(const KeyboardEvent& keyEvent) override;                                               // Runs when key events occur

	// Internal pass state
	RayLaunch::SharedPtr            mpRays;              // Our wrapper around a DX Raytracing pass
	RtScene::SharedPtr              mpScene;             // Our crazy spheres scene
	Camera::SharedPtr               mpCamera;            // Our scene camera
	CameraController::SharedPtr     mpCameraControl;     // The controller class for our camera
	TypedBufferBase::SharedPtr      mpGpuBufAABBs;       // The GPU buffer to store our sphere's bounding boxes
	TypedBufferBase::SharedPtr      mpGpuBufMatls;       // The GPU buffer to store our sphere's material properties

	// Default UI-controllable values
	int32_t                     mMaxDepth      = 5;      // Max ray recursion depth
	int32_t                     mNumSamples    = 1;      // Number of rays shot from camer per pixel in one frame
	bool                        mUseDoF        = true;   // Are we currently using depth of field?
	float                       mThinLensFocus = 6.0f;   // What's our camera's focal distance?
	float                       mFNum          = 128.0f; // Camera's f-number  

	// Toggles for optional scene paramters (not in Pete's 'Ray Tracing in One Weekend')
	bool   mShowDiffuseTextures = true;
	bool   mShowNormalMaps      = true;
	bool   mPerturbRefractions  = true;

	// An internal frame counter.  Used in HLSL to generate new random seeds each frame
	uint32_t                    mFrameCount = 0;

	// Override a function that provides information to the RenderPipeline 
	bool hasAnimation() override { return false; }       // Gets rid of a UI control that makes no sense for this demo

	///////////////////////////////////////////////////////////////////////////////////////
	//  Below here:  Functions, data, controls for setting up the random sphere scene
	///////////////////////////////////////////////////////////////////////////////////////
	//
	//     -> Remember, for DXR there are "triangles" and "custom intersections."  To
	//        use a custom intersection, your geometry is specified as bounding boxes.
	//     -> Also, because we're making the scene manually and using custom intersection
	//        (and Pete Shirley's shading system), we need to specify our materials 
	//        manually.  This adds a bit more complexity both to our C++ scene creation
	//        and also in the device-side HLSL, since we can't use Falcor helpers

	// A utility to build our random sphere scene roughly equivalent to the final scene
	//    from Pete Shirley's e-book "Ray Tracing in One Weekend"
	void buildRandomSphereScene();

	// Pick a random sphere location near distributed on a grid, near grid cell (xLoc, yLoc)
	vec3 randomSphereLocation(int xLoc, int yLoc);

	// We have 3 large, non-random spheres in our scene.  We don't want the random spheres too
	//     close to the big ones (i.e., we don't want them intersecting)
	bool isSphereLocationValid(const vec3 &pos);

	// A couple utilities to abstract insertion of our random spheres in the scene
	void addLambertianSphere(const vec3 &center);
	void addMetalSphere(const vec3 &center);
	void addGlassSphere(const vec3 &center);

	// A function that adds our large, non-random spheres to the scene
	void addLargeFixedSpheres();

	// The arrays of data we'll fill up to send to the GPU raytracer
	float *mpAABBs = nullptr;      // 6 floats / bounding box: (minX, minY, minZ, maxX, maxY, maxZ)
	float *mpMatls = nullptr;      // 4 floats / bounding box representing our custom material data
	uint32_t mCurSphereCount = 0;  // # of spheres we've added to the arrays above. 

	// Some textures we (might) apply on some of the spheres (see controls below)
	Texture::SharedPtr              mpEarthTex, mpMoonTex, mpNormalMap;

	// Some user-controllable variables for tweaking scene parameters
	float mRandomSphereRadius       = 0.2f;  // Not really designed to be changed 
	float mRandomSphereCellOffset   = 0.7f;  // How far off grid-aligned are our random spheres [0.0 = on grid]
	float mBigSphereProximityOffset = 1.1f;  // How far off the big spheres do our random ones need to be?
	bool  mAddTextureMappedSpheres  = true;  // Use Earth / Moon textures on some Lambertian spheres?
	float mTextureMapChance         = 0.1f;  // Texture mapping?  What is the probability for diffuse spheres?
	bool  mAddNormalMappedSpheres   = true;  // Add normal map to some metal spheres?
	float mNormalMapChance          = 0.25f; // Normal mapping?  What is the probability for metal spheres?
	bool  mGlossyRefraction         = true;  // Add a "glossy refraction" for refractive spheres?
	bool  mRandomIndexOrRefraction  = false; // Add randomness to the index of refraction for glass spheres?

	// Camera parameters
	vec3 mDefaultCameraPos = vec3(10.0f, 1.5f, 2.5f);
	vec3 mDefaultCameraUp = vec3(0.0f, 1.0f, 0.0f);
	vec3 mDefaultCameraAt = vec3(0.0f, 0.0f, 0.0f);
	
	// Since we're building a randomized scene, we need a random number generator.
	std::uniform_real_distribution<float> mRngDist;     // We're going to want random #'s in [0...1] (the default distribution)
	std::mt19937 mRng;                                  // Our random number generator.  Set up in buildRandomSphereScene()

	// Our GPU buffers containing AABBs and materials will be used:
	//   (a) Build a BVH for our ray tracer.  Falcor requires this to be a Vertex view of the resource
	//   (b) In our DXR HLSL shader.  This requires a ShaderResource view
	//   (c) For experimental code, I usually add UnorderedAccess view; it avoids lots of bugs when I make assumptions
	Resource::BindFlags mSceneBufferFlags = Resource::BindFlags::Vertex | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource;
};
