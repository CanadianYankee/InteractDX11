#define springblocksize 16

cbuffer cbSpringConfig : register (b0)
{
	uint g_iPattern;
	uint g_iModulus;
	uint g_iAntiModulus;
	uint g_iParticleCount;
};

RWTexture2D<uint> gOutput : register (u0);

[numthreads(springblocksize, springblocksize, 1)]
void ConfigSpringsCS(uint3 DTid : SV_DispatchThreadID)
{
	// Called for each pair of particles (i, j).  Result is an int determining the interaction.  
	// Possible values are:
	//   0 - out of range or i == j  (i.e., no force at all)
	//   1 - repulsive force
	//   2 - spring force (could be other positive values in the future)

	uint id1 = DTid.x;
	uint id2 = DTid.y;
	uint iddiff = (id1 > id2) ? id1 - id2 : (id2 - id1);
	uint spring = 1;

	switch(g_iPattern)
	{
	case 0: // Modulus
		if(iddiff > g_iParticleCount / 2)
		{
			iddiff = g_iParticleCount - iddiff;
		}
		spring = (iddiff % g_iModulus) == 0 ? 2 : 1;
		break;

	case 1: // String
		spring = (iddiff == g_iModulus) ? 2 : 1;
		break;

	case 2: // Sheet
		{
			bool bAttractive = false;

			uint w = g_iModulus;
			uint l = g_iParticleCount / g_iModulus;
			uint i1,j1,i2,j2;
			if(id1 < id2)
			{
				i1 = id1 % g_iModulus;
				j1 = id1 / g_iModulus;
				i2 = id2 % g_iModulus;
				j2 = id2 / g_iModulus;
			}
			else
			{
				i1 = id2 % g_iModulus;
				j1 = id2 / g_iModulus;
				i2 = id1 % g_iModulus;
				j2 = id1 / g_iModulus;
			}
			if((i1 < w - 1) && (j1 == j2) && (i2 == i1 + 1))
				bAttractive = true;
			else if((j1 < l - 1) && (j2 == j1 + 1))
			{
				if(i1 == i2)
					bAttractive = true;
				else if((j1 % 2 == 0) && (i1 > 0) && (i1 == i2 + 1))
					bAttractive = true;
				else if((j1 % 2 == 1) && (i1 < w - 1) && (i1 + 1 == i2))
					bAttractive = true;
			}

			spring = bAttractive ? 2 : 1;
			break;
		}
	}

	// Check for out-of-range
	if(id1 == id2 || id1 >= g_iParticleCount || id2 >= g_iParticleCount)
		spring = 0;

	// Write the result to the texture
	gOutput[DTid.xy] = spring;
}
