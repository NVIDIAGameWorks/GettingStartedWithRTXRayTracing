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

// Define pi
#define M_1_PI  0.318309886183790671538

// Generates a seed for a random number generator from 2 inputs plus a backoff
uint initRand(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// A work-around function because some DXR drivers seem to have broken atan2() implementations
float atan2_WAR(float y, float x)
{
	if (x > 0.f)
		return atan(y / x);
	else if (x < 0.f && y >= 0.f)
		return atan(y / x) + M_PI;
	else if (x < 0.f && y < 0.f)
		return atan(y / x) - M_PI;
	else if (x == 0.f && y > 0.f)
		return M_PI / 2.f;
	else if (x == 0.f && y < 0.f)
		return -M_PI / 2.f;
	return 0.f; // x==0 && y==0 (undefined)
}

// Convert our world space direction to a (u,v) coord in a latitude-longitude spherical map
float2 wsVectorToLatLong(float3 dir)
{
	float3 p = normalize(dir);

	// atan2_WAR is a work-around due to an apparent compiler bug in atan2
	float u = (1.f + atan2_WAR(p.x, -p.z) * M_1_PI) * 0.5f;
	float v = acos(p.y) * M_1_PI;
	return float2(u, v);
}

/** Apply normal map
*/
void clampApplyNormalMap(MaterialData m, inout ShadingData sd)
{
	uint mapType = EXTRACT_NORMAL_MAP_TYPE(m.flags);
	if (mapType == NormalMapUnused) return;

	float3 mapN = m.resources.normalMap.Sample(m.resources.samplerState, sd.uv).rgb;
	switch (mapType)
	{
	case NormalMapRGB:
		mapN = RgbToNormal(mapN);
		break;
	case NormalMapRG:
		mapN = RgToNormal(mapN.rg);
		break;
	default:
		return;
	}

	// Apply the transformation. Everything is normalized already
	sd.N = sd.T * mapN.x + sd.B * mapN.y + sd.N * mapN.z;
	sd.B = normalize(sd.B - sd.N * dot(sd.B, sd.N));
	sd.T = normalize(cross(sd.B, sd.N));
}

// This is a modification of the default Falcor routine in Shading.slang
ShadingData simplePrepareShadingData(VertexOut v, MaterialData m, float3 camPosW)
{
	ShadingData sd = initShadingData();

    ExplicitLodTextureSampler lodSampler = { 0 };  // Specify the tex lod/mip to use here

	// Sample the diffuse texture and apply the alpha test
	float4 baseColor = sampleTexture(m.resources.baseColor, m.resources.samplerState, v.texC, m.baseColor, EXTRACT_DIFFUSE_TYPE(m.flags), lodSampler);
	sd.opacity = m.baseColor.a;

	sd.posW = v.posW;
	sd.uv = v.texC;
	sd.V = normalize(camPosW - v.posW);
	sd.N = normalize(v.normalW);
	sd.B = normalize(v.bitangentW - sd.N * (dot(v.bitangentW, sd.N)));
	sd.T = normalize(cross(sd.B, sd.N));

	// We're reusing this in a badly-named way...
	sd.lightMap = sd.N;

	// Sample the spec texture
	float4 spec = sampleTexture(m.resources.specular, m.resources.samplerState, v.texC, m.specular, EXTRACT_SPECULAR_TYPE(m.flags), lodSampler);
	if (EXTRACT_SHADING_MODEL(m.flags) == ShadingModelMetalRough)
	{
		sd.diffuse = lerp(baseColor.rgb, float3(0), spec.b);
		sd.specular = lerp(float3(0.04f), baseColor.rgb, spec.b);
		sd.linearRoughness = spec.g;
	}
	else // if (EXTRACT_SHADING_MODEL(m.flags) == ShadingModelSpecGloss)
	{
		sd.diffuse = baseColor.rgb;
		sd.specular = spec.rgb;
		sd.linearRoughness = 1 - spec.a;
	}

	sd.linearRoughness = max(0.08, sd.linearRoughness); // Clamp the roughness so that the BRDF won't explode
	sd.roughness = sd.linearRoughness * sd.linearRoughness;
	sd.IoR = m.IoR;
	sd.doubleSidedMaterial = EXTRACT_DOUBLE_SIDED(m.flags);

	applyNormalMap(m, sd, lodSampler);
	sd.NdotV = dot(sd.N, sd.V);

	// Flip the normal if it's backfacing
	if (sd.NdotV <= 0 && sd.doubleSidedMaterial)
	{
		sd.N = -sd.N;
		sd.NdotV = -sd.NdotV;
	}

	return sd;
}
