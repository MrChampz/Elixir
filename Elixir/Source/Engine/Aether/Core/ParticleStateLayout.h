#pragma once

#include <Engine/Aether/Core/Particle.h>

namespace Elixir::Aether::Core
{
    // CoreV1 is six float4 values in both C++ and HLSL.
    constexpr uint32_t PARTICLE_STATE_CORE_V1_STRIDE = sizeof(glm::vec4) * 6;

    struct SParticleStateLayoutDescriptor
    {
        EParticleStateLayout Key = EParticleStateLayout::CoreV1;
        uint32_t ParticleStateStride = 0;
        uint32_t ParticleCapacity = 0;
    };

    // Immutable after renderer initialization. Each registered descriptor must
    // have a corresponding renderer runtime with compatible GPU resources,
    // shaders and pipelines.
    class ELIXIR_API ParticleStateLayoutRegistry final
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
