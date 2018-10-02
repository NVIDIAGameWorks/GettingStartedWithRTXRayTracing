// This render pass does the heavy lifting for the "Sphereflake" DXR sample.
//     If you understand this file and the corresponding .cpp file, you should understand the demo.

#pragma once
#include "../SharedUtils/RenderPass.h"   // The base class for all render passes in our app
#include "../SharedUtils/RayLaunch.h"    // The simple wrapper layer around DXR launches
#include <random>

class SphereflakeDemo : public RenderPass, inherit_shared_from_this<RenderPass, SphereflakeDemo>
{
public:
	// For consistency with other classes in Falcor / our tutorial code
    using SharedPtr = std::shared_ptr<SphereflakeDemo>;
    using SharedConstPtr = std::shared_ptr<const SphereflakeDemo>;

	// Constructors and destructors of various kinds
    static SharedPtr create() { return std::make_shared<SphereflakeDemo>(); }
	SphereflakeDemo() : RenderPass("Sphereflake", "Sphereflake Options") {}
	virtual ~SphereflakeDemo() = default;

protected:
	// Some user-controllable variables for tweaking scene parameters
	// recursion level:
	// 0: 2 spheres (including "ground-plane" sphere)
	// 1: 11 spheres
	// 2: 92
	// 3: 821
	// 4: 7,382
	// 5: 66,431
	// 6: 597,872
	// 7: 5,380,841
	// 8: 48,427,562
	// 9: 435,848,051 - this won't run, not enough memory
	int  mSizeFactor = 8;  // depth factor; 8 is stretching it, 9 is too much currently
	bool mShiny = true;  // are the spheres shiny or diffuse? 
	float mGroundSphereRadius = 1000; // largest reasonable is about 100000, tops

	// Camera parameters
	// make viewScale higher to move camera position away from scene and zoom in to compensate
	float mViewScale = 1.0f;
	vec3  mDefaultCameraPos = vec3(mViewScale *4.2f, mViewScale *3.4f, mViewScale *-2.6f);
	float mDefaultCameraFrameHeight = 15.0f / mViewScale;	// field of view, in mm for frame
	vec3  mDefaultCameraAt = vec3(0.0f, 0.0f, 0.0f);
	// close shadow (mGroundSphereRadius at around 10):
	//vec3  mDefaultCameraPos = vec3(0.488748, -0.492128, 0.839950);
	//vec3  mDefaultCameraAt = vec3(-0.152964, -1.11199, 1.2915796);

	vec3  mDefaultCameraUp = vec3(0.0f, 1.0f, 0.0f);
	//float mDefaultCameraFrameHeight = 30.0f;	// field of view, in mm for frame



	// Default UI-controllable values
	int32_t                     mMaxDepth = 5;			// Max ray recursion depth
	int32_t                     mNumSamples = 1;		// Number of rays shot from camera per pixel in one frame
	bool                        mUseDoF = false;		// Are we currently using depth of field?
	bool                        mProcTexture = false;	// Are we currently using the procedural texture for the plane?
	bool                        mHemiLight = false;		// Are we currently using the random hemisphere sampled lighting?
	float                       mThinLensFocus = 5.6f;  // What's our camera's focal distance?
	float						mAreaLightRadius = 0.0f;// Maximum diversion of surface->light ray, proportional to direction
	float                       mFNum = 20.0f;			// Camera's f-number; lower gives more depth-of-field effect when on

	//=====================================================
												
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

	vec3						mObjset[9];				// the nine axes from the sphere

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
	//     -> Also, because we're making the scene manually and using custom intersection,
	//        we need to specify our materials manually.  This adds a bit more complexity
	//        both to our C++ scene creation and also in the device-side HLSL, since we
	//        can't use Falcor helpers.

	// A utility to build a sphereflake, based on http://www.realtimerendering.com/resources/SPD/
	void buildScene();


	// A function that adds our large "ground-plane" sphere to the scene. A hack, easier than
	// making a separate ground-plane intersector. Good test for sphere intersection stability, too.
	void addGroundPlaneSphere(float offsetY);

	// make the sphereflake
	void SphereflakeDemo::createObjset();
	void SphereflakeDemo::makeSphereflake(int depth, vec3 &center, float radius, vec3 &direction, float scale);
	void SphereflakeDemo::lib_create_axis_rotate_matrix(tmat4x4<float, highp> &mx, vec3 &axis, float angle);
	void SphereflakeDemo::lib_transform_coord3(vec3 &vres, vec3 &vec, tmat4x4<float, highp> &mx);
	void SphereflakeDemo::lib_create_rotate_matrix(tmat4x4<float, highp> &mx, int axis, float angle);
	void SphereflakeDemo::lib_create_identity_matrix(tmat4x4<float, highp> &mx);
	void SphereflakeDemo::lib_zero_matrix(tmat4x4<float, highp> &mx);
	void SphereflakeDemo::addSphere(vec3 &center, float radius);


	// The arrays of data we'll fill up to send to the GPU raytracer
	float *mpAABBs = nullptr;      // 6 floats / bounding box: (minX, minY, minZ, maxX, maxY, maxZ)
	float *mpMatls = nullptr;      // 4 floats / bounding box representing our custom material data
	uint32_t mCurSphereCount = 0;  // # of spheres we've added to the arrays above. 
	
	// Since we're building a randomized scene, we need a random number generator.
	std::uniform_real_distribution<float> mRngDist;     // We're going to want random #'s in [0...1] (the default distribution)
	std::mt19937 mRng;                                  // Our random number generator.  Set up in buildRandomSphereScene()

	// Our GPU buffers containing AABBs and materials will be used:
	//   (a) Build a BVH for our ray tracer.  Falcor requires this to be a Vertex view of the resource
	//   (b) In our DXR HLSL shader.  This requires a ShaderResource view
	//   (c) For experimental code, I usually add UnorderedAccess view; it avoids lots of bugs when I make assumptions
	Resource::BindFlags mSceneBufferFlags = Resource::BindFlags::Vertex | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource;
};
