#include "epch.h"
#include "ParticleStateLayout.h"

namespace Elixir::Aether
{
    ParticleStateLayoutRegistry::ParticleStateLayoutRegistry(uint32_t particleCapacity)
    {
        const bool registered = Register({
            .Key = EParticleStateLayout::CoreV1,
            .ParticleStateStride = PARTICLE_STATE_CORE_V1_STRIDE,
            .ParticleCapacity = particleCapacity,
        });

        EE_CORE_ASSERT(
            registered,
            "Aether must register the CoreV1 particle state layout."
        )
    }

    bool ParticleStateLayoutRegistry::Register(const SParticleStateLayoutDescriptor descriptor)
    {
        if (descriptor.ParticleStateStride == 0 || descriptor.ParticleCapacity == 0)
            return false;

        if (Find(descriptor.Key))
            return false;

        m_Descriptors.push_back(descriptor);
        return true;
    }

    const SParticleStateLayoutDescriptor* ParticleStateLayoutRegistry::Find(
        const EParticleStateLayout key
    ) const
    {
        for (const auto& descriptor : m_Descriptors)
        {
            if (descriptor.Key == key)
                return &descriptor;
        }

        return nullptr;
    }
}
