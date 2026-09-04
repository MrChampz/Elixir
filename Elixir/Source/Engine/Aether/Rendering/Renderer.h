#pragma once

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Aether/Simulation/RenderFrame.h>

namespace Elixir::Aether::Rendering
{
    using namespace Core;
    using namespace Simulation;
    using namespace Materials;
    using namespace Materials::Rendering;

    struct alignas(16) SFrameData
    {
        glm::mat4 View;
        glm::mat4 Proj;
        glm::mat4 ViewProj;
        glm::vec3 CameraPos;
        float     Time  = 0.0f;
    };

    /**
     * @brief Stores statistics from the most recent rendering operation.
     *
     * These values describe the commands recorded by the most recent call to
     * Renderer::Render(). Renderer resets them before it renders another frame.
     */
    struct SRenderingMetrics
    {
        uint64_t SubmissionSerial = 0u;
        size_t   RenderBatchCount = 0;
        size_t   SubmittedRenderItemCount = 0;
        size_t   SubmittedMaterialCount = 0;
    };

    /**
     * @brief Records particle draw commands.
     *
     * Renderer reads an immutable RenderFrame produced by Simulator. It converts
     * the frame's particle items into a material render scene and records the
     * graphics commands.
     *
     * Renderer does not simulate particles, allocate simulation resources, or
     * submit command buffers.
     *
     * @thread_safety Use this class only from the render-frame thread.
     */
    class ELIXIR_API Renderer final
    {
      public:
        /**
         * @brief Creates the resources required to render particles.
         * @param context Graphics context that owns the rendering resources.
         * @pre context is not null and outlives the renderer.
         */
        explicit Renderer(const GraphicsContext* context);

        /**
         * @brief Builds material draw data for a simulated particle frame.
         *
         * The method updates the frame constants, creates a material render scene,
         * and returns it to the application-owned material system.
         *
         * @param frame Particle data produced by Simulator.
         * @param camera Camera used to transform and project the particles.
         * @return Material render scene.
         */
        MaterialRenderScene BuildRenderScene(
            const RenderFrame& frame,
            const Camera& camera
        );

        /**
         * @brief Returns statistics from the most recent rendering operation.
         * @return Statistics produced by the most recent call to Render().
         * @note A later call to Render() replaces these values.
         */
        const SRenderingMetrics& GetLastMetrics() const
        {
            return m_LastMetrics;
        }

      private:
        // Stores the vertex layouts used to render one particle-state layout.
        struct SParticleGraphicsLayout
        {
            EParticleStateLayout Key = EParticleStateLayout::CoreV1;
            BufferLayout SpriteVertexLayout;
            BufferLayout MeshVertexLayout;
        };

        // Creates the sprite and mesh vertex layouts for CoreV1 particles.
        void CreateCoreV1GraphicsLayout();

        // Creates the unit mesh geometry used by mesh particle rendering.
        void CreateMeshVertexBuffer();

        // Initializes per-frame constant-buffer data.
        void InitPerFrameData();

        // Finds the graphics layout for a particle-state layout.
        const SParticleGraphicsLayout* FindGraphicsLayout(EParticleStateLayout key) const;

        // Finds the particle-state resource for a layout in the render frame.
        static const SParticleStateRenderResource* FindRenderResource(
            const RenderFrame& frame,
            EParticleStateLayout key
        );

        // Converts particle render items into material geometry and draw commands.
        MaterialRenderScene BuildScene(const RenderFrame& frame) const;

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        std::vector<SParticleGraphicsLayout> m_GraphicsLayouts;

        uint32_t m_MeshVertexCount = 0;
        Ref<VertexBuffer> m_MeshVertexBuffer;

        SRenderingMetrics m_LastMetrics{};

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}
