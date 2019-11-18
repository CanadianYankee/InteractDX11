// Render particles pixel shader.  
// Input texture coordinates, field of view depth, and particle color.
// Output is texture color/alpha shaded with particle color and faded into the 
//    distance based on depth into the field of view.

#include "PhysicsVars.hlsli"

texture2D g_txParticleDraw;			// Image for each billboarded particle

SamplerState g_sampleTexture
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct PSRenderIn
{
	float2 texCoord	: TEXCOORD0;	// particle texture coordinates
	float depth		: TEXCOORD1;	// particle depth in field of view
	float4 color	: COLOR;		// particle color
};

float4 RenderPS(PSRenderIn input) : SV_TARGET
{
	float4 color = g_txParticleDraw.Sample(g_sampleTexture, input.texCoord) * input.color;
	color = saturate( 0.8 * color * g_fScale / input.depth);

	return color;
}
