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

// When loading a Falcor scene file, there's no guarantee the scene designer
//     added at least one camera or light.  Various places when designing
//     simple tutorials it's very easy to assume "scenes will always have 
//     a camera" or "scenes have at least one light."  
//
//  The loadScene() routine defined here does the following:
//     1) Opens a dialog box to query the user for which scene to load
//     2) Returns a nullptr if the user cancels our of the dialog box
//     3) Calls Falcor's internal scene I/O routines to load the scene 
//     4) Performs some sanity checking on the scene and defines some
//        reasonable defaults for lights and cameras if they don't exist
//        in the file.
//     5) On sucess, returns the resulting RtScene::SharedPtr

#pragma once

#include "Falcor.h"

// Load a scene, with an aspect ratio determined by the specified size.  If a filename is specified,
//    load that scene.  If no filename specified, a dialog box is opened so the user can select a file to load.
Falcor::RtScene::SharedPtr loadScene( uvec2 currentScreenSize, const char *defaultFilename = 0 );


// Opens a file dialog looking for textures.  Returns the full path name.
//     Parameter <isValid> is set to true if user selected a file, false otherwise.
std::string getTextureLocation(bool &isValid);