
#include "RayTracingInOneWeekendDemoPass.h"
#include <chrono>

namespace {
	// Where is our shader located?
	const char* kFileRayTrace = "RayTraceInAWeekend\\rayTracingInAWeekend.rt.hlsl";
};
 
// This callback gets run on program initialization
bool RayTracingInOneWeekendDemo::initialize(RenderContext::SharedPtr pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// When we have a rendering pipeline with multiple stages, our resource manager allows
	//    communication of buffers (and other data) between passes.  Here, that mostly means
	//    we need to output to the standardized output texture; request access to that.
	mpResManager = pResManager;
	mpResManager->requestTextureResource(ResourceManager::kOutputChannel);

	// Build our scene.  This is somewhat ugly, since spheres aren't currently abstracted
	//    in our framework (which was designed for raster... without analytic spheres)
	buildRandomSphereScene();

	// Create our wrapper around a ray tracing pass.  Tell it where our shaders are, then compile/link the program
	mpRays = RayLaunch::create(kFileRayTrace, "RayTracingInAWeekend");                // Specify our ray generation shader
	mpRays->addMissShader(kFileRayTrace, "ColorRayMiss");                             // Specify miss shader #0 
	mpRays->addMissShader(kFileRayTrace, "ShadowRayMiss");                            // Specify miss shader #1
	mpRays->addHitGroup(kFileRayTrace, "ColorRayClosestHit", "", "SphereIntersect");  // Specify shaders for hit group #0
	mpRays->compileRayProgram();                                                      // Compile our DXR shaders
	if (mpScene) mpRays->setScene(mpScene);                                           // Tell Falcor what scene we'll be tracing rays into

	// Our GUI needs more space than other passes, so enlarge the GUI window.
	setGuiSize(ivec2(250, 320));
	return true;
}

// This callback gets run whenever the GUI needs to be displayed
void RayTracingInOneWeekendDemo::renderGui(Gui* pGui)
{
	// Draw the widgets in our GUI.  This uses an immediate mode GUI (i.e., dear_imgui)
	int dirty = 0;
	dirty |= (int)pGui->addIntVar("spp / frame", mNumSamples, 1, 16);  
	dirty |= (int)pGui->addIntVar("ray depth", mMaxDepth, 1, 16); 
	dirty |= (int)pGui->addCheckBox(mUseDoF ? "using depth of field" : "no depth of field", mUseDoF);
	if (mUseDoF)
	{
		dirty |= (int)pGui->addFloatVar("f plane", mThinLensFocus, 0.01f, FLT_MAX, 0.01f); 
		dirty |= (int)pGui->addFloatVar("f number", mFNum, 1.0f, 512.0f, 0.01f);
	}
	pGui->addText("");
	pGui->addText("Optional scene parameters:");
	dirty |= (int)pGui->addCheckBox(mShowDiffuseTextures?"Textures on some diffuse spheres":"Using default Lambertian materials", mShowDiffuseTextures);
	dirty |= (int)pGui->addCheckBox(mShowNormalMaps ? "Normal maps on some metal spheres" : "Using default metal materials", mShowNormalMaps);
	dirty |= (int)pGui->addCheckBox(mPerturbRefractions ? "Perturbing refraction directions" : "Using default glass materials", mPerturbRefractions);

	// If any of our UI parameters changed, let the pipeline know (which resets accumulation)
	if (dirty) setRefreshFlag();
}

// This callback gets run whenever the frame needs to be re-rendered
void RayTracingInOneWeekendDemo::execute(RenderContext::SharedPtr pRenderContext)
{
	// Query our resource manager for our output buffer, so we render to the right texture.  Clear it.
	Texture::SharedPtr pOutTex = mpResManager->getTexture(ResourceManager::kOutputChannel);
	mpResManager->clearTexture(pOutTex, vec4(0, 0, 0, 0));

	// Check that we're ready to render
	if (!mpRays || !mpRays->readyToRender()) return;

	// If we've moved, make sure to update the camera for the current frame.  Also,
	//     if anything changed, tell the accumulation pass to clear the accumulation buffer
	if (mpCameraControl->update()) 
		setRefreshFlag();    

	// Grab a syntactic sugary wrapper around our DXR variables.  We're grabbing DXR's
	//    *global* variables here.  In the Falcor framework, all global variables in the HLSL 
	//    need to be labeled with the "shared" keyword.  (AFAIK, this is non-standard.)
	auto sharedVars = mpRays->getGlobalVars();

	// Send down our variables stored in the HLSL constant buffer "SharedCB."  
	sharedVars["SharedCB"]["gMinT"]            = float(1.0e-4f);            // Ray offset for numerical errors
	sharedVars["SharedCB"]["gMaxDepth"]        = uint32_t( mMaxDepth );     // Max recursive ray depth
	sharedVars["SharedCB"]["gFocalLen"]        = mThinLensFocus;            // Thin lens approx's focal length
	sharedVars["SharedCB"]["gLensRadius"]      = !mUseDoF ? 0.0f : float(mThinLensFocus / (2.0f * mFNum));
	sharedVars["SharedCB"]["gPixelMultiplier"] = 1.0f / float(mNumSamples); // For weighing multiple spp correctly
	sharedVars["SharedCB"]["gShowDiffuseTextures"] = mShowDiffuseTextures;
	sharedVars["SharedCB"]["gShowNormalMaps"]      = mShowNormalMaps;
	sharedVars["SharedCB"]["gPerturbRefractions"]  = mPerturbRefractions;

	// Send down our input and output textures to the HLSL shader
	sharedVars["gEarthTex"]   = mpEarthTex;
	sharedVars["gMoonTex"]    = mpMoonTex;
	sharedVars["gNormalMap"]  = mpNormalMap;
	sharedVars["gOutTex"]     = pOutTex;

	// Send down the HLSL Buffer<> variables used to store our scene geometry and materials. 
	sharedVars["gAABBData"]   = mpGpuBufAABBs;
	sharedVars["gMatlData"]   = mpGpuBufMatls;

	// Launch our ray tracing.  This is almost certainly *not* the best way to do multiple samples per pixel
	for (int i = 0; i < mNumSamples; i++)
	{
		// Update the frame count (gives different random numbers per sample)
		sharedVars["SharedCB"]["gFrameCount"] = mFrameCount++;

		// Lauch our rays!
		mpRays->execute(pRenderContext, mpResManager->getScreenSize(), mpCamera);
	}
}

// This callback gets executed whenever a mouse event occurs
bool RayTracingInOneWeekendDemo::processMouseEvent(const MouseEvent& mouseEvent)
{
	// Pass mouse events to our camera controller
	if (!mpCameraControl) return false;
	return mpCameraControl->onMouseEvent(mouseEvent);
}

// This callback gets executed whenever a key event occurs
bool RayTracingInOneWeekendDemo::processKeyEvent(const KeyboardEvent& keyEvent)
{
	// Pass keyboard events to our camera controller
	if (!mpCameraControl) return false;
	return mpCameraControl->onKeyEvent(keyEvent);
}

//////////////////////////////////////////////////////////////////////////////////////
//  Below here: utility methods for building our randomized scene
//////////////////////////////////////////////////////////////////////////////////////

// Main scene construction method
void RayTracingInOneWeekendDemo::buildRandomSphereScene()
{
	// Set up a random number generator by seeding it with the current time.  Why is using std::chrono so ugly? 
	auto currentTime = std::chrono::high_resolution_clock::now();
	auto timeInMillisec = std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime);
	mRng = std::mt19937(uint32_t(timeInMillisec.time_since_epoch().count()));

	// Load our texture resources (last 2 parameters in function calls: buildMimpaps? loadAsSRGB?)
	mpEarthTex  = Falcor::createTextureFromFile("Data/earth_2k.png",     true, true);
	mpMoonTex   = Falcor::createTextureFromFile("Data/moon_2k.png",      true, true);
	mpNormalMap = Falcor::createTextureFromFile("Data/normalMap_2k.png", true, true);

	// Allocate our arrays with enough space to contain all our random spheres, plus a little to spare.
	mpAABBs = new float[500 * 6];   // Space for 500 spheres.
	mpMatls = new float[500 * 4];   // Space for materials on each of the 500 spheres.

	// We're going to build a jittered grid of spheres (i.e., think 'one sphere 
	//    placed randomly in each grid cell').  The loop walks over this grid
	for (int i = -10; i <= 10; i++)
	{
		for (int j = -10; j <= 10; j++)
		{
			// Pick a random sphere center; make sure it's OK
			vec3 sphrCtr = randomSphereLocation(i, j);
			if (!isSphereLocationValid(sphrCtr))
				continue;

			// A random number is used to select this sphere's material
			float randMaterial = mRngDist(mRng);

			// 70% chance of Lambertian, 25% chance for Metal, 5% chance for Glass
			if (randMaterial < 0.7f)  
				addLambertianSphere(sphrCtr);
			else if (randMaterial < 0.95f)
				addMetalSphere(sphrCtr);
			else
				addGlassSphere(sphrCtr);
		}
	}

	// Add our big, fixed spheres to the scene
	addLargeFixedSpheres();

	////////////////////////////////////////////////////////////////////////////////////
	// Now we need to tell Falcor about our sphere scene so it can build the DirectX
	//    BVH acceleration structure to trace our rays.
	////////////////////////////////////////////////////////////////////////////////////

	// Create a GPU buffer containing our axis-aligned bounding boxes.  There are two vec3s per AABB 
	//     (i.e., 6 floats).  This is a TypedBuffer<vec3> so we can access in HLSL as "Buffer<float3>", 
	//     and we use the binding flags in mSceneBufferFlags (read comment in header for more details).
	mpGpuBufAABBs = TypedBuffer<vec3>::create(mCurSphereCount * 2, mSceneBufferFlags);
	mpGpuBufAABBs->updateData(mpAABBs, 0, mCurSphereCount * 6 * sizeof(float));  // Upload the data to GPU

	// Create a GPU buffer containing our materials.  (Our custom format has 4 floats per sphere)
	mpGpuBufMatls = TypedBuffer<vec4>::create(mCurSphereCount, mSceneBufferFlags);
	mpGpuBufMatls->updateData(mpMatls, 0, mCurSphereCount * 4 * sizeof(float));

	// Create a Falcor mesh object from our data.  We need to create a default Falcor material,
	//     that is never used, in order to use Falcor rendering calls.  Since Falcor hides a lot
	//     of nasty DXR API internals, this is a small price to pay.
	Material::SharedPtr defaultMatl = Material::create("DefaultMaterial");  // Never used

	// Actually create our mesh.  This code should get simplified once we figure out a better abstraction
	//     for creating non-triangular DXR geometry.  For now, this is kind of sloppy and experimental.
	auto pMesh = Mesh::createFromBoundingBoxBuffer(mpGpuBufAABBs, mCurSphereCount, defaultMatl);

	// Create a model in Falcor's scene abstraction from this mesh
	auto pModel = Model::create();                      // The model object
	pModel->addMeshInstance(pMesh, mat4());             // Add an instance of our mesh w/identity matrix
	auto pRtModel = RtModel::createFromModel(*pModel);  // For now, need special model with DXR-specific data

	// Finally create the final scene, we can be used with our tutorials' syntactic surgary wrappers
	mpScene = RtScene::createFromModel(pRtModel);

	// Since we didn't load from a file, this scene has no default camera.  So create one.
	mpCamera = Camera::create();
	mpCamera->setPosition(mDefaultCameraPos);
	mpCamera->setUpVector(mDefaultCameraUp);
	mpCamera->setTarget(mDefaultCameraAt);
	mpCamera->setAspectRatio(float(mpResManager->getDefaultFbo()->getWidth()) / float(mpResManager->getDefaultFbo()->getHeight()));
	mpCamera->setFocalLength(40.0f);          // In this app, this basically just controls field-of-view; I like this setting
	mpCamera->setDepthRange(0.001f, 1000.0f); // Should never affect ray tracing; give reasonable settings just in case

	// Attach a camera controller so our UI will update the camera.
	mpCameraControl = CameraController::SharedPtr(new FirstPersonCameraController);
	mpCameraControl->attachCamera(mpCamera);
}

// This function adds the 4 non-random spheres (one glass, one diffuse, one metal, and one to act as a ground plane)
void RayTracingInOneWeekendDemo::addLargeFixedSpheres()
{
	// Add a large sphere to act as a ground plane. (Radius = 100,000; Lambertian gray color)
	mpAABBs[6 * mCurSphereCount + 0] = -100000;  // AABB's min-x
	mpAABBs[6 * mCurSphereCount + 1] = -200000;  // AABB's min-y
	mpAABBs[6 * mCurSphereCount + 2] = -100000;  // AABB's min-z
	mpAABBs[6 * mCurSphereCount + 3] = 100000;   // AABB's max-x
	mpAABBs[6 * mCurSphereCount + 4] = 0;        // AABB's max-y
	mpAABBs[6 * mCurSphereCount + 5] = 100000;   // AABB's max-z
	mpMatls[4 * mCurSphereCount + 0] = 0.5f;  // Lambertian color (r)
	mpMatls[4 * mCurSphereCount + 1] = 0.5f;  // Lambertian color (g)
	mpMatls[4 * mCurSphereCount + 2] = 0.5f;  // Lambertian color (b)
	mpMatls[4 * mCurSphereCount + 3] = 0.0f;  // 0 means diffuse
	mCurSphereCount++;

	// Add a big glass ball around the origin with radius 1 and index or refraction 1.5
	mpAABBs[6 * mCurSphereCount + 0] = -1;
	mpAABBs[6 * mCurSphereCount + 1] = 0.01f;  // With radius 1, this will be offset slightly off the ground plane
	mpAABBs[6 * mCurSphereCount + 2] = -1;
	mpAABBs[6 * mCurSphereCount + 3] = 1;
	mpAABBs[6 * mCurSphereCount + 4] = 2.01f;
	mpAABBs[6 * mCurSphereCount + 5] = 1;
	mpMatls[4 * mCurSphereCount + 0] = 1.5f;  // Index of refraction
	mpMatls[4 * mCurSphereCount + 1] = 0.0f;  // Amount of glossines (none, so perfectly refractive)
	mpMatls[4 * mCurSphereCount + 2] = 0.0f;  // Not used
	mpMatls[4 * mCurSphereCount + 3] = 5.0f;  // 5 means refractive
	mCurSphereCount++;

	// Add a big diffuse ball (Center (-4,1,0), radius 1, Color (0.4,0.2,0.1))
	mpAABBs[6 * mCurSphereCount + 0] = -5;
	mpAABBs[6 * mCurSphereCount + 1] = 0;
	mpAABBs[6 * mCurSphereCount + 2] = -1;
	mpAABBs[6 * mCurSphereCount + 3] = -3;
	mpAABBs[6 * mCurSphereCount + 4] = 2;
	mpAABBs[6 * mCurSphereCount + 5] = 1;
	mpMatls[4 * mCurSphereCount + 0] = 0.4f;  // Lambertian color (red)
	mpMatls[4 * mCurSphereCount + 1] = 0.2f;  // (green)
	mpMatls[4 * mCurSphereCount + 2] = 0.1f;  // (blue)
	mpMatls[4 * mCurSphereCount + 3] = 0.0f;  // 0 means diffuse
	mCurSphereCount++;

	// Add a big metal ball (Center (4,1,0), radius 1, Reflectivity (0.7,0.6,0.5))
	mpAABBs[6 * mCurSphereCount + 0] = 3;
	mpAABBs[6 * mCurSphereCount + 1] = 0;
	mpAABBs[6 * mCurSphereCount + 2] = -1;
	mpAABBs[6 * mCurSphereCount + 3] = 5;
	mpAABBs[6 * mCurSphereCount + 4] = 2;
	mpAABBs[6 * mCurSphereCount + 5] = 1;
	mpMatls[4 * mCurSphereCount + 0] = 0.7f;  // Metal reflectivity (red)
	mpMatls[4 * mCurSphereCount + 1] = 0.6f;  // (green)
	mpMatls[4 * mCurSphereCount + 2] = 0.5f;  // (blue)
	mpMatls[4 * mCurSphereCount + 3] = 2.0f;  // 2 means perfectly specular metal
	mCurSphereCount++;
}

// Adds a random diffuse sphere at the specified center point
void RayTracingInOneWeekendDemo::addLambertianSphere(const vec3 &center)
{
	// Insert our sphere's bounding box into our list
	mpAABBs[6 * mCurSphereCount + 0] = center.x - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 1] = center.y - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 2] = center.z - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 3] = center.x + mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 4] = center.y + mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 5] = center.z + mRandomSphereRadius;

	// Pick a random color for the diffuse surface
	vec3 randColor = 0.8f * vec3(mRngDist(mRng), mRngDist(mRng), mRngDist(mRng)) + 0.1f;

	// If we want some diffuse spheres to be texture mapped, randomly decide if this one is
	//    textured and pick a random rotation (so textures aren't all oriented identically)
	bool  isTextureMapped = mAddTextureMappedSpheres && mRngDist(mRng) < mTextureMapChance;
	float texMapRotation = mRngDist(mRng);

	// Send our material parameters down.  For the 4th component, values in [0..1] mean diffuse,
	//    and values > 0 mean texture mapped with that non-zero rotation.
	mpMatls[4 * mCurSphereCount + 0] = randColor.x;
	mpMatls[4 * mCurSphereCount + 1] = randColor.y;
	mpMatls[4 * mCurSphereCount + 2] = randColor.z;
	mpMatls[4 * mCurSphereCount + 3] = isTextureMapped ? texMapRotation : 0.0f;

	// Ensure we put the next sphere in the right place
	mCurSphereCount++;
}

// Adds a random metal sphere at the specified center point
void RayTracingInOneWeekendDemo::addMetalSphere(const vec3 &center)
{
	// Insert our sphere's bounding box into our list
	mpAABBs[6 * mCurSphereCount + 0] = center.x - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 1] = center.y - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 2] = center.z - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 3] = center.x + mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 4] = center.y + mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 5] = center.z + mRandomSphereRadius;

	// Pick a random reflectance color for the metal material.
	vec3 randColor(0.5f*(1.0f + mRngDist(mRng)), 0.5f*(1.0f + mRngDist(mRng)), 0.5f*(1.0f + mRngDist(mRng)));

	// If we want some diffuse spheres to be normal mapped, randomly decide if this one should be.
	bool  isNormalMapped = mAddNormalMappedSpheres && mRngDist(mRng) < mNormalMapChance;

	// If we're not normal mapped, we will add a variable amount of perturbation to make some spheres more glossy
	float glossyPerturb = 0.8f*mRngDist(mRng);

	// Send our material parameters down.  For the 4th component, values in [2..4] mean metal.  3 means bump mapped
	//    and values [2..3) metal with a glossyPertub of [value-2]
	mpMatls[4 * mCurSphereCount + 0] = randColor.x;
	mpMatls[4 * mCurSphereCount + 1] = randColor.y;
	mpMatls[4 * mCurSphereCount + 2] = randColor.z;
	mpMatls[4 * mCurSphereCount + 3] = isNormalMapped ? 3.0f : 2.0f + glossyPerturb;

	// Ensure we put the next sphere in the right place
	mCurSphereCount++;
}

// Adds a random glass sphere at the specified center point
void RayTracingInOneWeekendDemo::addGlassSphere(const vec3 &center)
{
	// Insert our sphere's bounding box into our list
	mpAABBs[6 * mCurSphereCount + 0] = center.x - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 1] = center.y - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 2] = center.z - mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 3] = center.x + mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 4] = center.y + mRandomSphereRadius;
	mpAABBs[6 * mCurSphereCount + 5] = center.z + mRandomSphereRadius;

	// What index of refraction should this sphere use?
	float indexOfRefraction = mRandomIndexOrRefraction ? 1.2f + 0.6f * mRngDist(mRng) : 1.5f;

	// If we're not normal mapped, we will add a variable amount of perturbation to make some spheres more glossy
	float glossyPerturb = 0.1f*mRngDist(mRng);

	// Send our material parameters down.  For the 4th component, values of 5 mean refractive
	mpMatls[4 * mCurSphereCount + 0] = indexOfRefraction;
	mpMatls[4 * mCurSphereCount + 1] = mGlossyRefraction ? glossyPerturb : 0.0f;
	mpMatls[4 * mCurSphereCount + 2] = 0.0f;
	mpMatls[4 * mCurSphereCount + 3] = 5.0f;

	// Ensure we put the next sphere in the right place
	mCurSphereCount++;
}

// Picks a random sphere location (within a grid cell) given a grid center
vec3 RayTracingInOneWeekendDemo::randomSphereLocation(int xLoc, int yLoc)
{
	return vec3(float(xLoc + mRandomSphereCellOffset * mRngDist(mRng)), 
		        0.01f + mRandomSphereRadius, 
		        float(yLoc + mRandomSphereCellOffset * mRngDist(mRng)));
}

// Ensure we don't create any random spheres too close / inside the 3 larger, non-random spheres
bool RayTracingInOneWeekendDemo::isSphereLocationValid(const vec3 &pos)
{
	// Return false if we are too close to our big metal ball
	if (glm::length(pos - vec3(4.0f, mRandomSphereRadius, 0.0f)) < mBigSphereProximityOffset)
		return false;

	// Return false if we are too close to our big glass ball
	if (glm::length(pos - vec3(0.0f, mRandomSphereRadius, 0.0f)) < mBigSphereProximityOffset)
		return false;

	// Return false if we are too close to our big diffuse ball
	if (glm::length(pos - vec3(-4.0f, mRandomSphereRadius, 0.0f)) < mBigSphereProximityOffset)
		return false;

	// This position is good!
	return true;
}