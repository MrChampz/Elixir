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
        std::string DebugName;
        uint32_t EmitterIndex = 0;
        uint32_t LocalParticleOffset = 0;
        uint32_t ParticleCount = 0;
    };

    /**
     * @brief Publishes immutable particle render data for one submission.
     *
     * Simulator creates one frame after recording compute work and the required
     * compute-to-graphics barriers. Renderer consumes its resources and items
     * synchronously while recording graphics commands.
     *
     * RenderFrame owns frame-local metadata and strong references to shared GPU
     * resources. It never stores a command buffer or mutable SystemInstance
     * state.
     *
     * @thread_safety Immutable after construction. Concurrent readers are safe
     * when the referenced GPU wrappers are used according to their own contract.
     */
    class ELIXIR_API RenderFrame final
    {
    public:
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

        const std::vector<SParticleStateRenderResource>& GetResources() const { return m_Resources; }

        const Ref<DynamicStorageBuffer>& GetEmitterBuffer() const { return m_EmitterBuffer; }
        const std::vector<SRenderItem>& GetItems() const { return m_Items; }
        uint64_t GetSubmissionSerial() const { return m_SubmissionSerial; }
        float GetElapsedTimeSeconds() const { return m_ElapsedTimeSeconds; }

    private:
        std::vector<SParticleStateRenderResource> m_Resources;
        Ref<DynamicStorageBuffer> m_EmitterBuffer;
        std::vector<SRenderItem> m_Items;
        uint64_t m_SubmissionSerial = 0;
        float m_ElapsedTimeSeconds = 0.0f;
    };
}