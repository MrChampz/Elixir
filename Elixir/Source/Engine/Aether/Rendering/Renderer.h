#pragma once

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Material/MaterialRenderScene.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Aether/Simulation/RenderFrame.h>

namespace Elixir { class MaterialSystem; }

namespace Elixir::Aether::Rendering
{
    using namespace Core;
    using namespace Simulation;

    struct alignas(16) SFrameData
    {
        glm::mat4 View;
        glm::mat4 Proj;
        glm::mat4 ViewProj;
        glm::vec3 CameraPos;
        float     Time  = 0.0f;
    };

    struct SRenderingMetrics
    {
        uint64_t SubmissionSerial = 0u;
        size_t   RenderBatchCount = 0;
        size_t   SubmittedRenderItemCount = 0;
        size_t   SubmittedMaterialCount = 0;
    };

    /**
     * @brief Records material-based particle graphics passes.
     *
     * Renderer consumes an immutable RenderFrame produced by Simulator.
     * It owns graphics layouts, frame constants, mesh geometry, and
     * MaterialSystem scene construction. It does not own simulation state.
     *
     * @thread_safety Render-thread confined.
     */
    class ELIXIR_API Renderer final
    {
      public:
        Renderer(
            const GraphicsContext* context,
            MaterialSystem& materialSystem
        );

        void Render(
            const RenderFrame& frame,
            const Camera& camera,
            const Ref<CommandBuffer>& cmd
        );

        const SRenderingMetrics& GetLastMetrics() const
        {
            return m_LastMetrics;
        }

      private:
        struct SParticleGraphicsLayout
        {
            EParticleStateLayout Key = EParticleStateLayout::CoreV1;
            BufferLayout SpriteVertexLayout;
            BufferLayout MeshVertexLayout;
        };

        void CreateCoreV1GraphicsLayout();

        // Creates the unit mesh geometry used by mesh particle rendering.
        void CreateMeshVertexBuffer();

        // Initializes per-frame constant-buffer data.
        void InitPerFrameData();

        // Begins the graphics rendering scope for particle material passes.
        void BeginRendering(const Ref<CommandBuffer>& cmd) const;

        // Ends the graphics rendering scope for particle material passes.
        static void EndRendering(const Ref<CommandBuffer>& cmd);

        const SParticleGraphicsLayout* FindGraphicsLayout(EParticleStateLayout key) const;

        static const SParticleStateRenderResource* FindRenderResource(
            const RenderFrame& frame,
            EParticleStateLayout key
        );

        // Build material render items from submitted particle instances.
        MaterialRenderScene BuildMaterialRenderScene(const RenderFrame& frame) const;

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        std::vector<SParticleGraphicsLayout> m_GraphicsLayouts;

        uint32_t m_MeshVertexCount = 0;
        Ref<VertexBuffer> m_MeshVertexBuffer;

        MaterialSystem& m_MaterialSystem;

        SRenderingMetrics m_LastMetrics{};

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}
