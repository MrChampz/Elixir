#pragma once

#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Renderer/RenderPass.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::GUI
{
    /**
     * @brief One z-ordered draw call: the instance range an earlier
     * RenderPass::AppendRange produced for a single SBatchRun, plus the
     * pass that owns it.
     */
    struct SDrawItem
    {
        RenderPass* Pass;
        uint32_t FirstInstance;
        uint32_t InstanceCount;
    };

    struct SPerFrameData
    {
        glm::mat4 Proj;
    };

    class ELIXIR_API Renderer final
    {
      public:
        Renderer(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const Extent2D& extent
        );

        void Resize(const Extent2D& extent);

        /**
         * Regenerate each pass's GPU geometry from the batch (CPU build + vertex upload)
         * and rebuild the z-ordered draw item list used by Draw(). Only needs to run when
         * the batch changed; the passes retain their buffers otherwise.
         * @param batch the assembled frame batch.
         */
        void Rebuild(const RenderBatch& batch);

        /**
         * Record and submit the draw calls using each pass's current (cached) geometry.
         * Runs every frame — the GUI is re-composited over the scene each frame.
         */
        void Draw() const;

        void RegisterRenderPass(const Ref<RenderPass>& pass);

      private:
        void InitPerFrameData();
        void InitRenderPasses(const ShaderLoader* shaderLoader);

        void BeginRendering(const Ref<CommandBuffer>& cmd) const;
        void EndRendering(const Ref<CommandBuffer>& cmd) const;

        void CalculateProjectionMatrix();

        SPerFrameData m_PerFrameData{};
        Ref<UniformBuffer> m_PerFrameConstantBuffer;

        std::vector<Ref<RenderPass>> m_RenderPasses;
        std::unordered_map<EDrawCommandType, RenderPass*> m_PassesByType;

        std::vector<SDrawItem> m_DrawItems;

        float m_DPIScale = 1.0f;
        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}