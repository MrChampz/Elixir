#include "epch.h"
#include "MeshPassProcessor.h"

#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

namespace Elixir
{
    EMeshPass MeshPassProcessor::Classify(
        const Ref<const MaterialRenderProxy>& material,
        const std::optional<EMaterialBlendMode> overrideBlendMode)
    {
        if (overrideBlendMode)
        {
            switch (*overrideBlendMode)
            {
                case EMaterialBlendMode::Masked:
                    return EMeshPass::BaseMasked;
                case EMaterialBlendMode::Translucent:
                case EMaterialBlendMode::Additive:
                    return EMeshPass::Translucent;
                case EMaterialBlendMode::Opaque:
                default:
                    return EMeshPass::BaseOpaque;
            }
        }

        if (!material)
            return EMeshPass::BaseOpaque;

        const int alphaMode = (int)material->GetScalar("AlphaMode");
        if (alphaMode == 1)
            return EMeshPass::BaseMasked;
        if (alphaMode == 2
            && material->GetVector("BaseColorFactor").a < 0.95f)
        {
            return EMeshPass::Refraction;
        }
        return EMeshPass::BaseOpaque;
    }

    SMeshDrawCommand MeshPassProcessor::BuildCommand(
        const SModelPrimitive& primitive,
        const Ref<const MaterialRenderProxy>& material,
        const Ref<Shader>& defaultShader,
        const Ref<GraphicsPipeline>& defaultBasePipeline,
        const SMeshMaterialBinding* overrideBinding)
    {
        SMeshDrawCommand command;
        command.Vertices = primitive.Vertices;
        command.Indices = primitive.Indices;
        command.IndexCount = primitive.IndexCount;
        command.PushConstants.Model = primitive.Transform;
        command.PushConstants.MaterialIndex = primitive.MaterialIndex;
        command.SortOrigin = glm::vec3(primitive.Transform[3]);

        if (overrideBinding)
        {
            command.Pass = Classify(material, overrideBinding->BlendMode);
            command.Shader = overrideBinding->Shader;
            command.Pipeline = command.Pass == EMeshPass::Translucent
                ? overrideBinding->TranslucentPipeline
                : overrideBinding->BasePipeline;
        }
        else
        {
            command.Pass = Classify(material);
            command.Shader = defaultShader;
            // Built-in BLEND materials implement refraction in the shader and retain
            // the depth-writing base pipeline instead of enabling alpha blending.
            command.Pipeline = defaultBasePipeline;
        }

        return command;
    }

    Ref<GraphicsPipeline> MeshPassProcessor::CreatePipeline(
        const GraphicsContext* context, const Ref<Shader>& shader,
        const EMeshPass pass, const EMaterialBlendMode blendMode)
    {
        const BufferLayout layout({
            {
                {
                    { EDataType::Vec3, "Position" },
                    { EDataType::Vec3, "Normal"   },
                    { EDataType::Vec4, "Tangent"  },
                    { EDataType::Vec2, "TexCoord" },
                },
                EInputRate::Vertex
            }
        });

        PipelineBuilder builder;
        builder.SetShader(shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        builder.SetBufferLayout(layout);

        if (pass == EMeshPass::Translucent)
        {
            if (blendMode == EMaterialBlendMode::Additive)
                builder.EnableAdditiveBlending();
            else
                builder.EnableAlphaBlending();
        }
        else
        {
            builder.DisableBlending();
        }

        SPipelineCreateInfo info = builder.GetCreateInfo();
        info.DepthStencil.DepthTestEnable = true;
        info.DepthStencil.DepthWriteEnable = pass != EMeshPass::Translucent;
        info.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;
        return GraphicsPipeline::Create(context, info);
    }
}
