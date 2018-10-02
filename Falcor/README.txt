************************************************************************
*** Note:
***
*** This is the Falcor README created when prebuilding a binary package.
***     No need to worry about the contents unless you plan to use this 
***     Falcor distribution for your own project (not really recommended)
************************************************************************

Copy the contents of the Falcor Package directory to your project solution directory 

Go to property manager and add existing property sheet Falcor.props. It can be found in Framework/Source/ 

Add Falcor.lib to your linker dependencies 
	Project Properties -> Linker -> Input -> Additional Dependencies 
For each configuration, add the correct library directory to your Linker's additional lib directory 
	Project Properties -> Linker -> Gerneral -> Additional Library Directories
	The $(FALCOR_CORE_DIRECTORY) macro points to the framework dir and can be used here for convenience 
	For example, if you wanted a configuration to use the Debug Falcor with D3D12 back-end, you'd add
		$(FALCOR_CORE_DIRECTORY)\Lib\debugd3d12\

To quickly test if it worked
	Switch to a debug configuration 
	Include Falcor.h 
	add the following code 
		    Falcor::Renderer::UniquePtr mRenderer = std::make_unique<Falcor::Renderer>();
			Falcor::SampleConfig config;
			Falcor::Sample::run(config, mRenderer);
	and you should see a black window
	
Note: Falcor only supports x64! Will not work with x86 projects 
