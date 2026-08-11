#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Material/MaterialRenderProxy.h>
#include <Engine/Aether/Core/Particle.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Aether/Core/ResourceAllocation.h>

namespace Elixir::Aether::Simulation
{
    /**
     * @brief Retains the particle-state buffer for one simulation layout.
     *
     * Renderer selects the resource whose layout matches a render item. The
     * strong buffer reference keeps persistent simulation data alive while the
     * frame command buffer is recorded.
     */
    struct SParticleStateRenderResource
    {
        Core::EParticleStateLayout Layout = Core::EParticleStateLayout::CoreV1;
        Ref<StorageBuffer> ParticleStateBuffer;
    };

    /**
     * @brief Describes one immutable particle draw produced by Simulator.
     *
     * The item contains resolved allocation offsets, material state, transform,
     * and emitter-local draw information. Renderer converts it into a
     * MaterialRenderScene item without reading mutable system-instance state.
     */
    struct SRenderItem
    {
        Core::SSystemInstanceAllocation Allocation;
        Core::EParticleStateLayout ParticleStateLayout = Core::EParticleStateLayout::CoreV1;
        Core::EParticleRenderMode RenderMode = Core::EParticleRenderMode::Sprite;
        Ref<const MaterialRenderProxy> Material;
        glm::mat4 WorldTransform{ 1.0f };
        uint32_t EmitterIndex = 0;
        uint32_t LocalParticleOffset = 0;
        uint32_t ParticleCount = 0;
    };

    /**
     * @brief Provides immutable particle render data for one simulated frame.
     *
     * Simulator creates a RenderFrame after recording particle simulation work and
     * the required compute-to-graphics barriers. Renderer consumes the frame while
     * recording the corresponding graphics commands.
     *
     * The frame owns its render items and metadata. It also retains strong
     * references to the GPU buffers required by those items. It does not retain a
     * command buffer, frame submission, or mutable system-instance state.
     *
     * @thread_safety Immutable after construction. Concurrent reads are safe when
     * the referenced GPU resources are used according to their synchronization
     * requirements.
     */
    class ELIXIR_API RenderFrame final
    {
    public:
        /**
         * @brief Creates an immutable frame from resolved simulation output.
         *
         * @param resources Particle-state buffers paired with their layouts.
         * @param emitterBuffer Buffer containing the resolved emitter data.
         * @param items Particle draw items generated for the frame.
         * @param submissionSerial Serial number of the source frame submission.
         * @param elapsedTimeSeconds Total simulation time at frame publication.
         */
        RenderFrame(
            std::vector<SParticleStateRenderResource> resources,
            Ref<DynamicStorageBuffer> emitterBuffer,
            std::vector<SRenderItem> items,
            const uint64_t submissionSerial,
            const float elapsedTimeSeconds
        ) : m_Resources(std::move(resources)),
            m_EmitterBuffer(std::move(emitterBuffer)),
            m_Items(std::move(items)),
            m_SubmissionSerial(submissionSerial),
            m_ElapsedTimeSeconds(elapsedTimeSeconds) {}

        RenderFrame(const RenderFrame&) = delete;
        RenderFrame& operator=(const RenderFrame&) = delete;
        RenderFrame(RenderFrame&&) = delete;
        RenderFrame& operator=(RenderFrame&&) = delete;

        /**
         * @brief Returns the particle-state resources required for rendering.
         * @return Particle-state buffers paired with their layouts.
         */
        const std::vector<SParticleStateRenderResource>& GetResources() const { return m_Resources; }

        /**
         * @brief Returns the emitter data buffer used by the frame.
         * @return Strong reference to the emitter buffer.
         */
        const Ref<DynamicStorageBuffer>& GetEmitterBuffer() const { return m_EmitterBuffer; }

        /**
         * @brief Returns the resolved particle draw items.
         * @return Immutable collection of render items.
         */
        const std::vector<SRenderItem>& GetItems() const { return m_Items; }

        /**
         * @brief Returns the serial number of the source frame submission.
         * @return The serial number of the submission that produced this frame.
         */
        uint64_t GetSubmissionSerial() const { return m_SubmissionSerial; }

        /**
         * @brief Returns the accumulated simulation time for this frame.
         * @return Elapsed simulation time, in seconds.
         */
        float GetElapsedTimeSeconds() const { return m_ElapsedTimeSeconds; }

    private:
        std::vector<SParticleStateRenderResource> m_Resources;
        Ref<DynamicStorageBuffer> m_EmitterBuffer;
        std::vector<SRenderItem> m_Items;
        uint64_t m_SubmissionSerial = 0;
        float m_ElapsedTimeSeconds = 0.0f;
    };
}