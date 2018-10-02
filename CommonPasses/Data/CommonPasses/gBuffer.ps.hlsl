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

// Falcor / Slang imports to include shared code and data structures
__import Shading;           // Imports ShaderCommon and DefaultVS, plus material evaluation
__import DefaultVS;         // VertexOut declaration

struct GBuffer
{
	float4 wsPos    : SV_Target0;
	float4 wsNorm   : SV_Target1;
	float4 matDif   : SV_Target2;
	float4 matSpec  : SV_Target3;
	float4 matExtra : SV_Target4;
};

// Our main entry point for the g-buffer fragment shader.
GBuffer main(VertexOut vsOut, uint primID : SV_PrimitiveID, float4 pos : SV_Position)
{
	// This is a Falcor built-in that extracts data suitable for shading routines
	//     (see ShaderCommon.slang for the shading data structure and routines)
	ShadingData hitPt = prepareShadingData(vsOut, gMaterial, gCamera.posW);

	// Check if we hit the back of a double-sided material, in which case, we flip
	//     normals around here (so we don't need to when shading)
	float NdotV = dot(normalize(hitPt.N.xyz), normalize(gCamera.posW - hitPt.posW));
	if (NdotV <= 0.0f && hitPt.doubleSidedMaterial)
		hitPt.N = -hitPt.N;

	// Dump out our G buffer channels
	GBuffer gBufOut;
	gBufOut.wsPos    = float4(hitPt.posW, 1.f);
	gBufOut.wsNorm   = float4(hitPt.N, length(hitPt.posW - gCamera.posW) );
	gBufOut.matDif   = float4(hitPt.diffuse, hitPt.opacity);
	gBufOut.matSpec  = float4(hitPt.specular, hitPt.linearRoughness);
	gBufOut.matExtra = float4(hitPt.IoR, hitPt.doubleSidedMaterial ? 1.f : 0.f, 0.f, 0.f);

	return gBufOut;
}


