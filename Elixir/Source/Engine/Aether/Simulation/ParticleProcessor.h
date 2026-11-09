#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Graphics/Buffer.h>

namespace Elixir
{
    class ShaderLoader;
}
namespace Elixir::Aether
{
    struct SCompiledEmitter;
}

namespace Elixir::Aether::Simulation
{
    /** Supplies one emitter's particle allocation to an application particle stage. */
    struct SParticleProcessingContext
    {
        /** Recording command buffer; the processor must not submit or end it. */
        Ref<CommandBuffer> Command;
        /** Writable particle storage shared with Aether simulation and rendering. */
        Ref<StorageBuffer> Particles;
        /** Binary layout of the supplied particle buffer. */
        Core::EParticleStateLayout Layout = Core::EParticleStateLayout::CoreV1;
        /** First particle belonging to this emitter in the shared buffer. */
        uint32_t ParticleOffset = 0;
        /** Number of particle slots owned by this emitter. */
        uint32_t ParticleCount = 0;
        /** Duration of this simulation step, in seconds. */
        float DeltaSeconds = 0;
        /** Transform from the emitter's simulation coordinates to world coordinates. */
        glm::mat4 WorldTransform{1};
    };

    /** Owns application GPU resources for one Simulator, used on its render thread. */
    class ELIXIR_API ParticleProcessorRuntime
    {
      public:
        /** @brief Releases application GPU resources. */
        virtual ~ParticleProcessorRuntime();

        /**
         * @brief Records particle processing after integration and built-in constraints.
         * @param context Allocation and step data valid for this call.
         * @return Whether processing was recorded successfully.
         * @pre Retain uploads and resources until their graphics frame slot is reusable.
         * Do not change descriptors already referenced by recorded commands.
         */
        virtual bool Record(const SParticleProcessingContext& context) = 0;

      protected:
        /** @brief Creates a runtime implemented by an application. */
        ParticleProcessorRuntime();
    };

    /** Describes an optional application stage usable with any emitter render mode. */
    class ELIXIR_API ParticleProcessor
    {
      public:
        /** @brief Releases the immutable processor description. */
        virtual ~ParticleProcessor();

        /**
         * @brief Checks whether an emitter satisfies the application's processing contract.
         * @param emitter Compiled emitter whose particle ranges and operations are available.
         * @return Whether this processor can handle the emitter.
         */
        virtual bool IsCompatible(const SCompiledEmitter& emitter) const = 0;

        /**
         * @brief Creates resources owned by one Simulator instance.
         * @param context Graphics context that outlives the returned runtime.
         * @param loader Shader loader available only during this call.
         * @return The runtime, or null when its resources cannot be created.
         */
        virtual Scope<ParticleProcessorRuntime> CreateRuntime(
            const GraphicsContext* context,
            const ShaderLoader& loader
        ) const = 0;

      protected:
        /** @brief Creates an immutable application processor description. */
        ParticleProcessor();
    };
}
