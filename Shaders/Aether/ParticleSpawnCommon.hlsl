#ifndef AETHER_PARTICLE_SPAWN_COMMON_HLSL
#define AETHER_PARTICLE_SPAWN_COMMON_HLSL

struct SGPUSpawnRequest
{
    uint SpawnCursor;
    uint SpawnCount;
    uint EmissionIndex;
    uint Generation;
};

#endif
