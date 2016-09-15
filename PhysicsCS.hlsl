cbuffer cbWorldPhysics : register(b0)
{
	float g_fScale;
	float g_fParticleRadius;
	float g_fFrictionCoeff;
	float g_fSpeedLimit;

	float g_fSpringLength;
	float g_fSpringConstant;
	float wpfDummy0;
	float wpfDummy1;

	uint g_iParticleCount;
	uint g_iNumBlocks;
	uint g_bCentralForce;
	uint wpiDummy0;
};

cbuffer cbFrameVariables : register(b1)
{
	float4x4 g_matWorld;
	float4x4 g_matView;
	float4x4 g_matProjection;
	float g_fGlobalTime;
	float g_fElapsedTime;
	float fvfDummy0;
	float fvfDummy1;
};

struct PosVel
{
	float4 pos;
	float4 vel;
};

Texture2D<uint> springTable : register(t0);
StructuredBuffer<PosVel> oldPosVel : register(t1);

RWStructuredBuffer<PosVel> newPosVel : register(u0);

#define blocksize 128
groupshared float4 sharedPos[blocksize];

// Calculate the force on particle 1 from particle 2, accumlated into "force"
void Calculate2BodyForce(inout float3 force, uint id1, float3 pos1, uint id2, float3 pos2)
{
	float3 delta = pos2 - pos1;

	switch(springTable[uint2(id1, id2)])
	{
	case 1: // Repulsive force
		{
			float mag = 8.0 * dot(delta, delta) / (g_fScale * g_fScale);
			force += -0.5 * g_fSpringConstant * exp(-mag) * normalize(delta);
		}
		break;
		
	case 2: // Basic spring
		force += g_fSpringConstant * (delta - g_fSpringLength * normalize(delta));
		break;

	case 0: // No interaction (out-of-bounds or id1 == id2)
	default:
		break;
	}
}

void CalcFixed2BodyForce(inout float3 force, uint id1, float3 pos1, uint id2, float3 pos2)
{
	if(id2 != id1)
	{
		uint iddiff = (id1 > id2) ? id1 - id2 : (id2 - id1);

		if(iddiff > g_iParticleCount / 2)
		{
			iddiff = g_iParticleCount - iddiff;
		}

		float3 delta = pos2 - pos1;
		if(iddiff % 123 == 0)
		{
			force += g_fSpringConstant * (delta - g_fSpringLength * normalize(delta));
		}
		else
		{
			float mag = 8.0 * dot(delta, delta) / (g_fScale * g_fScale);
			force += -0.5 * g_fSpringConstant * exp(-mag) * normalize(delta);
		}
	}
}

[numthreads(blocksize, 1, 1)]
void PhysicsCS( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	// Called once for each particle.
	float3 pos = oldPosVel[DTid.x].pos.xyz;
	float3 vel = oldPosVel[DTid.x].vel.xyz;
	float3 force = 0;

#if true
	// Jump through memory in blocksize tiles, calculating the force due to other particles.
	for(uint tile = 0; tile < g_iNumBlocks; tile++)
	{
		// Cache a tile of particles into shared memory
		uint id2 = tile * blocksize;
		sharedPos[GI] = oldPosVel[tile * blocksize + GI].pos;

		GroupMemoryBarrierWithGroupSync();

		// Count through the tile eight at a time
		for(uint counter = 0; counter < blocksize; counter += 8)
		{
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter].xyz);
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter+1].xyz);
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter+2].xyz);
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter+3].xyz);
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter+4].xyz);
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter+5].xyz);
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter+6].xyz);
			Calculate2BodyForce(force, DTid.x, pos, id2++, sharedPos[counter+7].xyz);
		}
	}
#else
	uint id1 = DTid.x;
	for(uint id2 = 0; id2 < g_iParticleCount; id2++)
	{
		Calculate2BodyForce(force, DTid.x, pos, id2, oldPosVel[id2].pos.xyz);
	}
#endif

	// Apply the central force if requested
	if(g_bCentralForce != 0)
	{
		float central = dot(pos, pos);
		central = 0.1f * g_fSpringConstant * central / (g_fScale * g_fScale);
		force -= central * pos;
	}

	// Calculate the frictional force, including the non-linear part due to the "speed limit"
	float3 friction = (-g_fFrictionCoeff - (0.2 * dot(vel, vel)) / (g_fSpeedLimit * g_fSpeedLimit * g_fSpeedLimit)) * vel;

	// Calculate the new velocity, capping it at big values
	vel += friction + g_fElapsedTime * force;

	if(g_fSpeedLimit > 0 && length(vel) > 20.0 * g_fSpeedLimit)
	{
		vel = 20.0 * g_fSpeedLimit * normalize(vel);
	}

	// Calculate the new position
	pos += g_fElapsedTime * vel;

	// Store the new values
	if(DTid.x < g_iParticleCount)
	{
		newPosVel[DTid.x].pos = float4(pos, 1.0f);
		newPosVel[DTid.x].vel = float4(vel, 0.0f);
	}
}
