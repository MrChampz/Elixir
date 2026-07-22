#pragma once

#include <Engine/Aether/Particle.h>

namespace Elixir::Aether
{
    // CoreV1 is six float4 values in both C++ and HLSL.
    constexpr uint32_t PARTICLE_STATE_CORE_V1_STRIDE = sizeof(glm::vec4) * 6;

    struct SParticleStateLayoutDescriptor
    {
        EParticleStateLayout Key = EParticleStateLayout::CoreV1;
        uint32_t ParticleStateStride = 0;
        uint32_t ParticleCapacity = 0;
    };

    // Immutable after renderer initialization. A later phase will register
    // layout-specific shader and pipeline families against these descriptors.
    class ParticleStateLayoutRegistry final
    {
    public:
        explicit ParticleStateLayoutRegistry(uint32_t particleCapacity);

        bool Register(SParticleStateLayoutDescriptor descriptor);

        const SParticleStateLayoutDescriptor* Find(EParticleStateLayout key) const;

        const std::vector<SParticleStateLayoutDescriptor>& GetDescriptors() const { return m_Descriptors;}

    private:
        std::vector<SParticleStateLayoutDescriptor> m_Descriptors;
    };
}