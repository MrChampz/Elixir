#include "epch.h"
#include "MaterialRenderer.h"

#include <Engine/Graphics/Pipeline/PipelineBuilder.h>

namespace Elixir
{
    MaterialRenderer::MaterialRenderer(
        const GraphicsContext* context,
        Ref<DynamicStorageBuffer> frameBuffer,
        const MaterialTextureRegistry& textures
    ) : m_FrameBuffer(std::move(frameBuffer)),
        m_Textures(textures),
        m_Context(context) {}

    EMaterialUsage MaterialRenderer::GetUsage(EMaterialPass pass)
    {
        switch (pass)
        {
            case EMaterialPass::ParticleSprite: return EMaterialUsage::ParticleSprite;
            case EMaterialPass::ParticleRibbon: return EMaterialUsage::ParticleRibbon;
            case EMaterialPass::ParticleMesh:   return EMaterialUsage::ParticleMesh;
        }

        EE_CORE_ASSERT(false, "Material pass does not have a material usage.")
        return EMaterialUsage::ParticleSprite;
    }

    std::optional<SMaterialProgramKey> MaterialRenderer::GetProgramKey(
        const EMaterialPass pass,
        const MaterialRenderProxy& material
    ) const
    {
        const auto usage = GetUsage(pass);
        const auto compiled = material.GetCompiledMaterial();
        if (!compiled || !compiled->SupportsUsage(usage))
            return std::nullopt;

        const auto& shader = compiled->GetShader(usage);
        if (!shader)
            return std::nullopt;

        return SMaterialProgramKey{ .Identity = shader.get() };
    }

    std::optional<SPreparedMaterialPass> MaterialRenderer::Prepare(
        const SMaterialPassRequest& request
    )
    {
        if (!request.Material || !request.Pipeline.VertexLayout)
            return std::nullopt;

        const auto program = GetProgramKey(request.Pass, *request.Material);
        if (!program)
            return std::nullopt;

        const auto compiled = request.Material->GetCompiledMaterial();
        const auto& shader = compiled->GetShader(GetUsage(request.Pass));

        if (!BindDescriptorResources(shader, request))
            return std::nullopt;

        if (!request.InitialPushConstants.empty())
        {
            shader->SetPushConstant(
                "pc",
                const_cast<void*>(static_cast<const void*>(request.InitialPushConstants.data())),
                request.InitialPushConstants.size()
            );
        }

        return SPreparedMaterialPass{
            .Shader = shader,
            .Pipeline = GetPipeline(request.Pass, shader, request.Pipeline),
        };
    }

    Ref<GraphicsPipeline> MaterialRenderer::GetPipeline(
        const EMaterialPass pass,
        const Ref<Shader>& shader,
        const SMaterialPipelineRequest& request
    )
    {
        const SPipelineKey key{
            .Pass = pass,
            .Shader = shader.get(),
            .VertexLayoutKey = request.VertexLayoutKey,
        };

        if (const auto found = m_Pipelines.find(key); found != m_Pipelines.end())
            return found->second;

        PipelineBuilder builder;
        builder.SetShader(shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        builder.SetBufferLayout(*request.VertexLayout);

        switch (pass)
        {
            case EMaterialPass::ParticleSprite:
                builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
                builder.EnableAlphaBlending();
                builder.DisableDepthTest();
                break;
            case EMaterialPass::ParticleRibbon:
                builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
                builder.EnableAlphaBlendingMax();
                builder.DisableDepthTest();
                break;
            case EMaterialPass::ParticleMesh:
                builder.SetCullMode(ECullMode::Back, EFrontFace::CounterClockwise);
                builder.EnableAlphaBlendingMax();
                break;
        }

        auto info = builder.GetCreateInfo();

        if (pass == EMaterialPass::ParticleMesh)
        {
            info.DepthStencil.DepthTestEnable = true;
            info.DepthStencil.DepthWriteEnable = true;
            info.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;
        }

        const auto pipeline = GraphicsPipeline::Create(m_Context, info);
        m_Pipelines.emplace(key, pipeline);
        return pipeline;
    }

    bool MaterialRenderer::BindDescriptorResources(
        const Ref<Shader>& shader,
        const SMaterialPassRequest& request
    )
    {
        SDescriptorBindingState state{
            .Pass = request.Pass,
        };

        state.ExternalResources.reserve(
            request.ExternalResources.GetResourceCount()
        );

        for (const auto& binding : request.ExternalResources.ConstantBuffers)
        {
            state.ExternalResources.push_back({
                .Name = std::string(binding.Name),
                .Resource = binding.Buffer.get(),
                .Type = EDescriptorBindingType::ConstantBuffer,
            });
        }

        for (const auto& binding : request.ExternalResources.StorageBuffers)
        {
            std::visit(
                [&state, &binding](const auto& buffer)
                {
                    using TBuffer = std::remove_cvref_t<decltype(buffer)>;

                    state.ExternalResources.push_back({
                        .Name = std::string(binding.Name),
                        .Resource = buffer.get(),
                        .Type = std::is_same_v<TBuffer, Ref<StorageBuffer>>
                            ? EDescriptorBindingType::StorageBuffer
                            : EDescriptorBindingType::DynamicStorageBuffer
                    });
                },
                binding.Buffer
            );
        }

        const auto found = m_DescriptorBindings.find(shader.get());
        if (found != m_DescriptorBindings.end())
        {
            if (found->second != state)
            {
                EE_CORE_ERROR("Material shader descriptor bindings changed after initialization.")
                return false;
            }

            return true;
        }

        for (const auto& binding : request.ExternalResources.ConstantBuffers)
        {
            shader->BindConstantBuffer(std::string(binding.Name), binding.Buffer);
        }

        for (const auto& binding : request.ExternalResources.StorageBuffers)
        {
            std::visit(
                [&shader, &binding](const auto& buffer)
                {
                    shader->BindStorageBuffer(std::string(binding.Name), buffer);
                },
                binding.Buffer
            );
        }

        shader->BindStorageBuffer("materials", m_FrameBuffer);
        shader->BindTextureSet("sprites", m_Textures.GetTextureSet());
        shader->BindSampler("spriteSampler", m_Textures.GetSampler());

        m_DescriptorBindings.emplace(shader.get(), std::move(state));
        return true;
    }
}
