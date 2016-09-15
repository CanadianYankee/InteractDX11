// Render particles vertex shader.  
// Input is particle number and color.
// Output is particle position (extracted from structured buffer maintained by the 
//    physics compute shader) in untransformed space - input color is passed on unchanged.

struct PosVelo
{
    float4 pos;
    float4 velo;
};

StructuredBuffer<PosVelo>   g_bufPosVelo;

// Particle input to vertex shader 
struct VSRenderIn
{
	float4	color	: COLOR;		// particle color
	uint	id		: SV_VERTEXID;	// auto-generated vertex id
};

// Particle output from vertex shader
struct VSRenderOut
{
	float3 position	: POSITION;		// particle 3D position
	float4 color	: COLOR;		// particle color
};

// Just look up our particle positions and pass them on to the 
// geometry shader - all world transformations will happen there.
VSRenderOut RenderVS( VSRenderIn input )
{
	VSRenderOut output;

	// Look up the particle position in the texture map
	output.position = g_bufPosVelo[input.id].pos.xyz;
	output.color = input.color;

	return output;
}
