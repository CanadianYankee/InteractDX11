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

