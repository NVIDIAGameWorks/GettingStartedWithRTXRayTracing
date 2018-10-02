
#include "SphereflakeDemoPass.h"
#include <chrono>

namespace {
	// Where is our shader located?
	const char* kFileRayTrace = "Sphereflake\\sphereflake.rt.hlsl";
};
 
// This callback gets run on program initialization
bool SphereflakeDemo::initialize(RenderContext::SharedPtr pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// When we have a rendering pipeline with multiple stages, our resource manager allows
	//    communication of buffers (and other data) between passes.  Here, that mostly means
	//    we need to output to the standardized output texture; request access to that.
	mpResManager = pResManager;
	mpResManager->requestTextureResource(ResourceManager::kOutputChannel);

	// Build our scene.  This is somewhat ugly, since spheres aren't currently abstracted
	//    in our framework (which was designed for raster... without analytic spheres)
	buildScene();

	// Create our wrapper around a ray tracing pass.  Tell it where our shaders are, then compile/link the program
	mpRays = RayLaunch::create(kFileRayTrace, "Sphereflake");			              // Specify our ray generation shader
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
void SphereflakeDemo::renderGui(Gui* pGui)
{
	// Draw the widgets in our GUI.  This uses an immediate mode GUI (i.e., dear_imgui)
	int dirty = 0;
	dirty |= (int)pGui->addIntVar("spp / frame", mNumSamples, 1, 16);  
	dirty |= (int)pGui->addIntVar("ray depth", mMaxDepth, 1, 16);
	dirty |= (int)pGui->addCheckBox(mHemiLight ? "use hemi-light" : "no hemi-light", mHemiLight);
	if (!mHemiLight) {
		dirty |= (int)pGui->addFloatVar("light rad.", mAreaLightRadius, 0.0f, 8.0f, 0.005f);
	}
	dirty |= (int)pGui->addCheckBox(mUseDoF ? "using depth of field" : "no depth of field", mUseDoF);
	if (mUseDoF)
	{
		dirty |= (int)pGui->addFloatVar("f plane", mThinLensFocus, 0.01f, FLT_MAX, 0.01f); 
		dirty |= (int)pGui->addFloatVar("f number", mFNum, 1.0f, 512.0f, 0.1f);
	}
	pGui->addText("");
	pGui->addText("Optional scene parameters:");
	dirty |= (int)pGui->addCheckBox(mProcTexture ? "use texture" : "no texture", mProcTexture);
	//dirty |= (int)pGui->addCheckBox(mShowDiffuseTextures?"Textures on some diffuse spheres":"Using default Lambertian materials", mShowDiffuseTextures);
	//dirty |= (int)pGui->addCheckBox(mShowNormalMaps ? "Normal maps on some metal spheres" : "Using default metal materials", mShowNormalMaps);
	//dirty |= (int)pGui->addCheckBox(mPerturbRefractions ? "Perturbing refraction directions" : "Using default glass materials", mPerturbRefractions);

	// If any of our UI parameters changed, let the pipeline know (which resets accumulation)
	if (dirty) setRefreshFlag();
}

// This callback gets run whenever the frame needs to be re-rendered
void SphereflakeDemo::execute(RenderContext::SharedPtr pRenderContext)
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
	//sharedVars["SharedCB"]["gShowDiffuseTextures"] = mShowDiffuseTextures;
	//sharedVars["SharedCB"]["gShowNormalMaps"]      = mShowNormalMaps;
	//sharedVars["SharedCB"]["gPerturbRefractions"]  = mPerturbRefractions;
	sharedVars["SharedCB"]["gAreaLightRadius"] = mAreaLightRadius;          // Perpendicular radius of directional area lights
	sharedVars["SharedCB"]["gProcTexture"] = mProcTexture ? 1.0f : 0.0f;	// 1.0 means use procedural texture
	sharedVars["SharedCB"]["gHemiLight"] = mHemiLight ? 1.0f : 0.0f;	// 1.0 means use cosine-weighted hemispherical light

	// Send down our input and output textures to the HLSL shader
	//sharedVars["gEarthTex"]   = mpEarthTex;
	//sharedVars["gMoonTex"]    = mpMoonTex;
	//sharedVars["gNormalMap"]  = mpNormalMap;
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
bool SphereflakeDemo::processMouseEvent(const MouseEvent& mouseEvent)
{
	// Pass mouse events to our camera controller
	if (!mpCameraControl) return false;
	return mpCameraControl->onMouseEvent(mouseEvent);
}

// This callback gets executed whenever a key event occurs
bool SphereflakeDemo::processKeyEvent(const KeyboardEvent& keyEvent)
{
	// Pass keyboard events to our camera controller
	if (!mpCameraControl) return false;
	return mpCameraControl->onKeyEvent(keyEvent);
}

//////////////////////////////////////////////////////////////////////////////////////
//  Below here: utility methods for building our randomized scene
//////////////////////////////////////////////////////////////////////////////////////

// Main scene construction method
void SphereflakeDemo::buildScene()
{
	// Set up a random number generator by seeding it with the current time.  Why is using std::chrono so ugly? 
	auto currentTime = std::chrono::high_resolution_clock::now();
	auto timeInMillisec = std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime);
	mRng = std::mt19937(uint32_t(timeInMillisec.time_since_epoch().count()));

	// Load our texture resources (last 2 parameters in function calls: buildMimpaps? loadAsSRGB?)
	//mpEarthTex  = Falcor::createTextureFromFile("Data/earth_2k.png",     true, true);
	//mpMoonTex   = Falcor::createTextureFromFile("Data/moon_2k.png",      true, true);
	//mpNormalMap = Falcor::createTextureFromFile("Data/normalMap_2k.png", true, true);

	vec3 center(0.0f, 0.0f, 0.0f);
	vec3 direction(0.0f, 0.0f, 1.0f);
	float radius = 0.5f;
	float scale = 1.0f / 3.0f;	// interesting to change to 1/2

	uint32_t sz = 1 + 1;	// one for the ground plane
	uint32_t onLevel = 9;
	for ( int i = 0; i < mSizeFactor; i++) {
		sz += onLevel;
		onLevel *= 9;
	}
	wchar_t szBuff[1024];
	swprintf_s(szBuff, 1024, L"number of spheres: %d", sz);
	OutputDebugString(szBuff);

	// Allocate our arrays with enough space to contain all our random spheres, plus a little to spare.
	mpAABBs = new float[sz * 6];   // Space for sz spheres.
	mpMatls = new float[sz * 4];   // Space for materials on each of the sz spheres.

	createObjset();

	makeSphereflake(mSizeFactor, center, radius, direction, scale);

	// Add our big, fixed sphere to the scene
	addGroundPlaneSphere(-0.5f);

	assert(mCurSphereCount <= sz);

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
	mpCamera->setFrameHeight(mDefaultCameraFrameHeight);
	mpCamera->setAspectRatio(float(mpResManager->getDefaultFbo()->getWidth()) / float(mpResManager->getDefaultFbo()->getHeight()));
	mpCamera->setFocalLength(40.0f);          // In this app, this basically just controls field-of-view; I like this setting
	mpCamera->setDepthRange(0.001f, 1000.0f); // Should never affect ray tracing; give reasonable settings just in case

	// Attach a camera controller so our UI will update the camera.
	mpCameraControl = CameraController::SharedPtr(new FirstPersonCameraController);
	mpCameraControl->attachCamera(mpCamera);
}

// This function adds the 4 non-random spheres (one glass, one diffuse, one metal, and one to act as a ground plane)
void SphereflakeDemo::addGroundPlaneSphere(float offsetY)
{
	// Add a large sphere to act as a ground plane. (Radius = 100,000; Lambertian gray color)
	mpAABBs[6 * mCurSphereCount + 0] = -mGroundSphereRadius;  // AABB's min-x
	mpAABBs[6 * mCurSphereCount + 1] = -(mGroundSphereRadius*2)+offsetY;  // AABB's min-y
	mpAABBs[6 * mCurSphereCount + 2] = -mGroundSphereRadius;  // AABB's min-z
	mpAABBs[6 * mCurSphereCount + 3] = mGroundSphereRadius;   // AABB's max-x
	mpAABBs[6 * mCurSphereCount + 4] = offsetY;        // AABB's max-y
	mpAABBs[6 * mCurSphereCount + 5] = mGroundSphereRadius;   // AABB's max-z
	mpMatls[4 * mCurSphereCount + 0] = 1.00f;  // Lambertian color (r)
	mpMatls[4 * mCurSphereCount + 1] = 0.75f;  // Lambertian color (g)
	mpMatls[4 * mCurSphereCount + 2] = 0.33f;  // Lambertian color (b)
	mpMatls[4 * mCurSphereCount + 3] = 0.0f;  // 0 means diffuse
	mCurSphereCount++;
}

void SphereflakeDemo::createObjset()
{
	vec3    axis, tempPt, trioDir[3];
	float   dist;
	tmat4x4<float, highp> mx;
	int     numSet, numVert;

	dist = 1.0f / sqrt(2.0f);

	trioDir[0] = vec3(dist, dist, 0.0f);
	trioDir[1] = vec3(dist, 0.0f, -dist);
	trioDir[2] = vec3(0.0f, dist, -dist);

	axis = vec3(1.0f, -1.0f, 0.0f);
	axis = normalize(axis);
	lib_create_axis_rotate_matrix(mx, axis,
		asin((2.0f / sqrt(6.0f))));

	for (numVert = 0; numVert<3; ++numVert) {
		lib_transform_coord3(tempPt, trioDir[numVert], mx);
		trioDir[numVert] = tempPt;
	}

	for (numSet = 0; numSet<3; ++numSet) {
		lib_create_rotate_matrix(mx, 2, numSet*2.0f*(float)M_PI / 3.0f);
		for (numVert = 0; numVert<3; ++numVert) {
			lib_transform_coord3(mObjset[numSet * 3 + numVert],
				trioDir[numVert], mx);
		}
	}
}

// Code derived from the Standard Procedural Databases code, http://www.realtimerendering.com/resources/SPD/
void SphereflakeDemo::makeSphereflake(int depth, vec3 &center, float radius, vec3 &direction, float scale)
{
	float   angle;
	vec3    axis, zAxis;
	vec3    childPt, childDir;
	tmat4x4<float,highp>  mx;
	int     numVert;
	float   childScale, childRadius;

	// output sphere at location & radius defined by center.
	// rotate 90 degrees along X axis, as +Y is up in this scene, while sphereflake code computes object positions with +Z up;
	// this "rotation hack" below works only because largest shiny sphere is centered at the origin.
	vec3 zcenter;
	zcenter.x = center.x;
	zcenter.y = center.z;
	zcenter.z = -center.y;
	addSphere(zcenter, radius);

	// check if children should be generated
	if (depth > 0) {
		--depth;

		// rotation matrix to new axis from +Y axis
		if (direction.z >= 1.0f) {
			lib_create_identity_matrix(mx);
		}
		else if (direction.z <= -1.0f) {
			lib_create_rotate_matrix(mx, 1, (float)M_PI);
		}
		else {
			zAxis = vec3(0.0f, 0.0f, 1.0f);
			axis = cross(zAxis,direction);
			axis = normalize(axis);
			angle = acos(dot(zAxis, direction));
			lib_create_axis_rotate_matrix(mx, axis, angle);
		}

		// scale down location of new spheres
		childScale = radius * (1.0f + scale);

		for (numVert = 0; numVert < 9; ++numVert) {
			// only do progress for top-level objects of recursion
			lib_transform_coord3(childPt, mObjset[numVert], mx);
			childPt.x = childPt.x * childScale + center.x;
			childPt.y = childPt.y * childScale + center.y;
			childPt.z = childPt.z * childScale + center.z;
			// scale down radius
			childRadius = radius * scale;
			childDir = childPt - center;
			childDir.x /= childScale;
			childDir.y /= childScale;
			childDir.z /= childScale;
			makeSphereflake(depth, childPt, childRadius, childDir, scale);
		}
	}
}

#define SQR(x) ((x)*(x))

void SphereflakeDemo::lib_create_axis_rotate_matrix(tmat4x4<float, highp> &mx, vec3 &axis, float angle)
{
	float  cosine, one_minus_cosine, sine;

	cosine = cos(angle);
	sine = sin(angle);
	one_minus_cosine = 1.0f - cosine;

	mx[0].x = SQR(axis.x) + (1.0f - SQR(axis.x)) * cosine;
	mx[0].y = axis.x * axis.y * one_minus_cosine + axis.z * sine;
	mx[0].z = axis.x * axis.z * one_minus_cosine - axis.y * sine;
	mx[0].w = 0.0f;

	mx[1].x = axis.x * axis.y * one_minus_cosine - axis.z * sine;
	mx[1].y = SQR(axis.y) + (1.0f - SQR(axis.y)) * cosine;
	mx[1].z = axis.y * axis.z * one_minus_cosine + axis.x * sine;
	mx[1].w = 0.0f;

	mx[2].x = axis.x * axis.z * one_minus_cosine + axis.y * sine;
	mx[2].y = axis.y * axis.z * one_minus_cosine - axis.x * sine;
	mx[2].z = SQR(axis.z) + (1.0f - SQR(axis.z)) * cosine;
	mx[2].w = 0.0f;

	mx[3].x = 0.0f;
	mx[3].y = 0.0f;
	mx[3].z = 0.0f;
	mx[3].w = 1.0f;
}

void SphereflakeDemo::lib_transform_coord3(vec3 &vres, vec3 &vec, tmat4x4<float, highp> &mx)
{
	vec3 vtemp;
	vtemp.x = vec.x * mx[0].x + vec.y * mx[1].x + vec.z * mx[2][0]; // + vec.w * mx[3][0];
	vtemp.y = vec.x * mx[0].y + vec.y * mx[1].y + vec.z * mx[2][1]; // + vec.w * mx[3][1];
	vtemp.z = vec.x * mx[0].z + vec.y * mx[1].z + vec.z * mx[2][2]; // + vec.w * mx[3][2];
	//vtemp.w = vec.x * mx[0].w + vec.y * mx[1].w + vec.z * mx[2].w + vec.w * mx[3][3];
	vres = vtemp;
}

void SphereflakeDemo::lib_create_rotate_matrix(tmat4x4<float, highp> &mx, int axis, float angle)
{
	float cosine, sine;

	lib_zero_matrix(mx);
	cosine = cos(angle);
	sine = sin(angle);
	switch (axis) {
	case 0:
		mx[0].x = 1.0f;
		mx[1].y = cosine;
		mx[2].z = cosine;
		mx[1].z = sine;
		mx[2].y = -sine;
		break;
	case 1:
		mx[1].y = 1.0f;
		mx[0].x = cosine;
		mx[2].z = cosine;
		mx[2].x = sine;
		mx[0].z = -sine;
		break;
	case 2:
		mx[2].z = 1.0f;
		mx[0].x = cosine;
		mx[1].y = cosine;
		mx[0].y = sine;
		mx[1].x = -sine;
		break;
	default:
		fprintf(stderr, "Internal Error: bad call to lib_create_rotate_matrix\n");
		exit(1);
		break;
	}
	mx[3].w = 1.0f;
}

void SphereflakeDemo::lib_zero_matrix(tmat4x4<float, highp> &mx)
{
	int i;

	for (i = 0; i < 4; ++i) {
		mx[i].x = 0.0f;
		mx[i].y = 0.0f;
		mx[i].z = 0.0f;
		mx[i].w = 0.0f;
	}
}

void SphereflakeDemo::lib_create_identity_matrix(tmat4x4<float, highp> &mx)
{
	lib_zero_matrix(mx);
	mx[0].x =
	mx[1].y =
	mx[2].z =
	mx[3].w = 1.0f;
}

void SphereflakeDemo::addSphere(vec3 &center, float radius)
{
	// Insert our sphere's bounding box into our list
	mpAABBs[6 * mCurSphereCount + 0] = center.x - radius;
	mpAABBs[6 * mCurSphereCount + 1] = center.y - radius;
	mpAABBs[6 * mCurSphereCount + 2] = center.z - radius;
	mpAABBs[6 * mCurSphereCount + 3] = center.x + radius;
	mpAABBs[6 * mCurSphereCount + 4] = center.y + radius;
	mpAABBs[6 * mCurSphereCount + 5] = center.z + radius;

	// Pick a reflectance color for the metal material.
	vec3 someColor(0.5f, 0.5f, 0.5f);

	float glossyPerturb = 3.5f;

	// Send our material parameters down.  For the 4th component, values in [2..4] mean metal.  3 means bump mapped
	//    and values [2..3) metal with a glossyPerturb of [value-2]
	if (mShiny) {
		mpMatls[4 * mCurSphereCount + 0] = someColor.x;
		mpMatls[4 * mCurSphereCount + 1] = someColor.y;
		mpMatls[4 * mCurSphereCount + 2] = someColor.z;
		mpMatls[4 * mCurSphereCount + 3] = glossyPerturb;
	}
	else {
		mpMatls[4 * mCurSphereCount + 0] = 0.33f;  // Lambertian color (r)
		mpMatls[4 * mCurSphereCount + 1] = 0.75f;  // Lambertian color (g)
		mpMatls[4 * mCurSphereCount + 2] = 1.00f;  // Lambertian color (b)
		mpMatls[4 * mCurSphereCount + 3] = 0.0f;  // 0 means diffuse
	}

	// Ensure we put the next sphere in the right place
	mCurSphereCount++;
}
