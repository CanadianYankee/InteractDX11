// Render particles geometry shader.  
// Input is list of point positions in untransformed coordinates.
// Output is a billboard quad facing the camera, along with a depth number for use 
//    in dimming far-away particles - input color is passed through unchanged.

#include "PhysicsVars.hlsli"

cbuffer cbConstants
{
    static float2 g_quadPositions[4] =
    {
        float2( -1, 1 ),
        float2( 1, 1 ),
        float2( -1, -1 ),
        float2( 1, -1 ),
    };

	static float2 g_quadTexcoords[4] = 
	{
        float2(0,0), 
        float2(1,0),
        float2(0,1),
        float2(1,1)
	};
};

struct GSRenderIn
{
	float3 position	: POSITION;		// particle 3D position
	float4 color	: COLOR;		// particle color
};

struct GSRenderOut
{
	float2 texCoord	: TEXCOORD0;	// particle texture coordinates
	float  depth	: TEXCOORD1;	// particle depth in field of view
	float4 color	: COLOR;		// particle color
	float4 position	: SV_POSITION;	// screen position
};

// Generate a quad billboard (facing viewer) for each point
[maxvertexcount(4)]
void RenderGS(point GSRenderIn input[1], inout TriangleStream< GSRenderOut > SpriteStream)
{
	GSRenderOut output;
	float4x4 preWorldView = mul(g_matWorld, g_matView);

	float4 center = mul(float4(input[0].position, 1.0), preWorldView);
	center /= center.w;

	// Make two triangles to billboard the particle texture sprite
	for(int i = 0; i < 4; i++)
	{
		float4 offset = float4(g_quadPositions[i]*g_fParticleRadius, 0.0, 1.0);
		output.position = mul(offset + center, g_matProjection);
		output.color = input[0].color;
		output.texCoord = g_quadTexcoords[i];
		output.depth = center.z;
		SpriteStream.Append(output);
	}
	SpriteStream.RestartStrip();
}
