#pragma once

#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/Material/MaterialGraph.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>

#include <optional>

namespace Elixir
{
    enum class EMeshPass : uint8_t
    {
        BaseOpaque,
        BaseMasked,
        Translucent,
        Refraction
    };

    struct SMeshPushConstants
    {
        glm::mat4 Model;
        uint32_t MaterialIndex = 0;
    };

    // Shader and pipelines selected for a graph-material override. A null binding
    // means the renderer's built-in material path remains authoritative.
    struct SMeshMaterialBinding
    {
        Ref<Shader> Shader;
        Ref<GraphicsPipeline> BasePipeline;
        Ref<GraphicsPipeline> TranslucentPipeline;
        EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
    };

    // Immutable portion of one indexed draw. Frame data and transparent ordering
    // remain dynamic; this record can be cached until its material slot changes.
    struct SMeshDrawCommand
    {
        Ref<Shader> Shader;
        Ref<GraphicsPipeline> Pipeline;
        Ref<VertexBuffer> Vertices;
        Ref<IndexBuffer> Indices;
        SMeshPushConstants PushConstants;
        glm::vec3 SortOrigin{ 0.0f };
        uint32_t IndexCount = 0;
        EMeshPass Pass = EMeshPass::BaseOpaque;
    };

    // Converts a static-mesh primitive plus material state into a pass-specific draw
    // command. It also owns the pipeline state contract for every supported mesh pass.
    class ELIXIR_API MeshPassProcessor
    {
      public:
        static EMeshPass Classify(const Ref<const MaterialRenderProxy>& material,
            std::optional<EMaterialBlendMode> overrideBlendMode = std::nullopt);

        static SMeshDrawCommand BuildCommand(const SModelPrimitive& primitive,
            const Ref<const MaterialRenderProxy>& material,
            const Ref<Shader>& defaultShader,
            const Ref<GraphicsPipeline>& defaultBasePipeline,
            const SMeshMaterialBinding* overrideBinding = nullptr);

        static Ref<GraphicsPipeline> CreatePipeline(const GraphicsContext* context,
            const Ref<Shader>& shader, EMeshPass pass,
            EMaterialBlendMode blendMode = EMaterialBlendMode::Translucent);

        [[nodiscard]] static bool IsBasePass(const EMeshPass pass)
        {
            return pass == EMeshPass::BaseOpaque || pass == EMeshPass::BaseMasked;
        }

        [[nodiscard]] static bool RequiresSorting(const EMeshPass pass)
        {
            return pass == EMeshPass::Translucent || pass == EMeshPass::Refraction;
        }

        [[nodiscard]] static bool RequiresSceneColor(const EMeshPass pass)
        {
            return pass == EMeshPass::Refraction;
        }
    };
}
