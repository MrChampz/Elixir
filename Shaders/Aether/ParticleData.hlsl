#ifndef AETHER_PARTICLE_DATA_HLSL
#define AETHER_PARTICLE_DATA_HLSL

struct SGPUParticle
{
    float3 Position;
    float  Size;
    float3 Velocity;
    float  Age;
    float4 Transform;       // x = rotation, y = scale
    float3 Tangent;
    uint   RibbonId;
    float4 Color;
    uint   EmitterIndex;
    uint   LinkOrder;
    float  Lifetime;
    float  Alive;
};

struct SGPUEmitter
{
    uint  LocalParticleOffset;
    uint  MaxParticles;
    uint  SpawnOperationOffset;
    uint  SpawnOperationCount;
    uint  UpdateOperationOffset;
    uint  UpdateOperationCount;
    uint  TriggerTargetOffset;
    uint  TriggerTargetCount;
    uint  RenderMode;
    float SpawnRatePerSecond;
    float GravityScale;
    float BurstIntervalSeconds;
    uint  BurstCount;
    uint  IsTriggerDriven;
    uint  KillOnLifetimeExpired;
    uint  Padding1;
    uint  StrandUpdateOperationOffset;
    uint  StrandUpdateOperationCount;
    uint  Padding2;
    uint  Padding3;
};

struct SGPUSystemInstance
{
    uint ParticleBaseOffset;
    uint EmitterBaseOffset;
    uint OperationBaseOffset;
    uint OperandBaseOffset;
    uint ConstantBaseOffset;  // Shared literal word offset in constants.
    uint ParameterBaseOffset; // Instance parameter word offset in parameters.

    uint EmitterStateBaseOffset;
    uint SpawnRequestBaseOffset;
    uint TriggerEventBaseOffset;
    uint TriggerQueueStateBaseOffset;

    uint ParticleCount;
    uint EmitterCount;
    uint TriggerEventCapacityPerEmitter;
    uint Generation;

    uint ParticleStateLayoutIndex;
};

#endif
