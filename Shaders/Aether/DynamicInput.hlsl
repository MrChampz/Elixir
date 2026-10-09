#ifndef AETHER_DYNAMIC_INPUT_HLSL
#define AETHER_DYNAMIC_INPUT_HLSL

static const uint DynamicInput_None          = 0u;
static const uint DynamicInput_DeltaTime     = 1u;
static const uint DynamicInput_NormalizedAge = 2u;
static const uint DynamicInput_EmitterTime   = 3u;
static const uint DynamicInput_Random        = 4u;
static const uint DynamicInput_ParticleSeed  = 5u;

/** @brief Stores the runtime values available to dynamic operands. */
struct SDynamicInputContext
{
    /** @brief Elapsed time in seconds for the current simulation step. */
    float DeltaTime;

    /** @brief Particle age divided by lifetime, clamped to [0, 1]. */
    float NormalizedAge;

    /** @brief Current simulation time in seconds. */
    float Time;

    /** @brief Random value in [0, 1) for this particle and simulation time. */
    float Random;

    /** @brief Stable seed derived from the particle's buffer index. */
    float ParticleSeed;
};

/** @brief Calculates the same dynamic random value for spawn and update. */
float CalculateDynamicInputRandom(float particleSeed, float time)
{
    return frac(sin((particleSeed * 91.37) + time * 0.71) * 43758.5453);
}

/** @brief Returns the selected runtime value, or one for None or an unknown input. */
float ResolveDynamicInput(uint inputType, SDynamicInputContext context)
{
    if (inputType == DynamicInput_DeltaTime)
        return context.DeltaTime;

    if (inputType == DynamicInput_NormalizedAge)
        return context.NormalizedAge;

    if (inputType == DynamicInput_EmitterTime)
        return context.Time;

    if (inputType == DynamicInput_Random)
        return context.Random;

    if (inputType == DynamicInput_ParticleSeed)
        return context.ParticleSeed;

    return 1.0;
}

#endif
