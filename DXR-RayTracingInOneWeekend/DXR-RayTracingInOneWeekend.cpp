
#include "Falcor.h"
#include "../SharedUtils/RenderingPipeline.h"
#include "Passes/RayTracingInOneWeekendDemoPass.h"
#include "Passes/SimpleAccumulationPass.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	// Create our rendering pipeline
	RenderingPipeline *pipeline = new RenderingPipeline();

	// Add passes into our pipeline
	//  * Pass 0 renders our "Ray Tracing in One Weekend" scene into the resource manager's <kOutputChannel> buffer
	//  * Pass 1 temporally accumulates samples in the <kOutputChannel> buffer over time (and outputs the the same channel)
	//  * After all passes are complete, our RenderingPipeline displays whatever is in the <kOutputChannel> buffer on screen
	pipeline->setPass(0, RayTracingInOneWeekendDemo::create());
	pipeline->setPass(1, SimpleAccumulationPass::create(ResourceManager::kOutputChannel)); 
	
	// Define a set of config / window parameters for our program
    SampleConfig config;
	config.windowDesc.title = 
		"DirectX Raytracing Tutorial:  Demonstrating final scene from Pete Shirley's 'Ray Tracing in One "
		"Weekend' book.  (Uses DXR intersection shaders with a scene consisting entirely of spheres.  "
		"Absolutely no triangles used in this rendering!)";	
	config.windowDesc.resizableWindow = true;
	config.windowDesc.width = 1920; 
	config.windowDesc.height = 1080; 

	// Start our program!
	RenderingPipeline::run(pipeline, config);
}
