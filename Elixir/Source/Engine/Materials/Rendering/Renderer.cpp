#include "epch.h"
#include "Renderer.h"

#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>

namespace Elixir::Materials::Rendering
{
    Renderer::Renderer(
        const GraphicsContext* context,
        const TextureRegistry& textures
    ) : m_Textures(textures),
        m_Context(context) {}

    std::optional<SPreparedPass> Renderer::Prepare(const SPassRequest& request)
    {
        if (!request.Material || !request.Pipeline.VertexLayout || !request.MaterialBuffer)
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

        return SPreparedPass{
            .Shader = shader,
            .Pipeline = GetPipeline(request.Pass, shader, request.Pipeline),
        };
    }

    SRenderResult Renderer::Record(const SMaterialSceneRecordRequest& request)
    {
        SRenderResult result{
            .MaterialCount = request.MaterialCount,
        };

        if (!request.CommandBuffer || !request.Scene || !request.MaterialBuffer)
            return result;

        std::vector<SBatch> batches;

        for (const auto& resolved : request.Items)
        {
            if (!resolved.Item || !resolved.Proxy) continue;

            const auto& item = *resolved.Item;
            const auto* geometry = request.Scene->FindGeometry(item.GeometryIndex);
            EE_CORE_ASSERT(geometry, "Material render item geometry is unavailable.");
            if (!geometry) continue;

            const auto program = GetProgramKey(item.Pass, *resolved.Proxy);
            EE_CORE_ASSERT(program, "Material render item does not support its requested pass.")
            if (!program) continue;

            const SBatchKey key{
                .Pass = item.Pass,
                .GeometryIndex = item.GeometryIndex,
                .Program = *program
            };

            auto batch = std::ranges::find_if(
                batches,
                [&key](const SBatch& candidate)
                {
                    return candidate.Key == key;
                }
            );

            if (batch == batches.end())
            {
                batches.push_back({ .Key = key });
                batch = std::prev(batches.end());
            }

            batch->Items.push_back(&resolved);
        }

        std::ranges::stable_sort(
            batches,
            [](const SBatch& left, const SBatch& right)
            {
                if (left.Key.Pass != right.Key.Pass)
                    return GetPassOrder(left.Key.Pass) < GetPassOrder(right.Key.Pass);

                if (left.Key.GeometryIndex != right.Key.GeometryIndex)
                    return left.Key.GeometryIndex < right.Key.GeometryIndex;

                return std::less<const void*>{}(
                    left.Key.Program.Identity,
                    right.Key.Program.Identity
                );
            }
        );

        for (const auto& batch : batches)
        {
            if (batch.Items.empty()) continue;

            const auto* geometry = request.Scene->FindGeometry(batch.Key.GeometryIndex);
            if (!geometry) continue;

            const auto& first = *batch.Items.front();
            const auto prepared = Prepare({
                .Pass = batch.Key.Pass,
                .Material = first.Proxy.get(),
                .Pipeline = geometry->Pipeline,
                .ExternalResources = {
                    .ConstantBuffers = geometry->ConstantBuffers,
                    .StorageBuffers = geometry->StorageBuffers,
                },
                .MaterialBuffer = request.MaterialBuffer,
                .InitialPushConstants = std::span{
                    first.Item->PushConstants.Data.data(),
                    first.Item->PushConstants.Size
                },
            });
            if (!prepared) continue;

            ++result.BatchCount;
            prepared->Pipeline->Bind(request.CommandBuffer);

            for (const auto& binding : geometry->VertexBuffers)
            {
                request.CommandBuffer->BindBuffer<VertexBuffer>(
                    binding.Buffer,
                    std::span<uint64_t>{},
                    1,
                    binding.Binding
                );
            }

            for (const auto* resolved : batch.Items)
            {
                const auto constants = resolved->Item->PushConstants.Resolve(
                    resolved->MaterialIndex
                );

                prepared->Shader->SetPushConstant(
                    request.CommandBuffer,
                    "pc",
                    const_cast<std::byte*>(constants.data()),
                    resolved->Item->PushConstants.Size
                );

                request.CommandBuffer->Draw(
                    resolved->Item->Draw.VertexCount,
                    resolved->Item->Draw.InstanceCount,
                    resolved->Item->Draw.FirstVertex,
                    resolved->Item->Draw.FirstInstance
                );

                ++result.DrawCount;
            }
        }

        return result;
    }

    std::optional<SProgramKey> Renderer::GetProgramKey(
        const EMaterialPass pass,
        const MaterialRenderProxy& material
    )
    {
        const auto usage = GetUsage(pass);
        const auto compiled = material.GetCompiledMaterial();
        if (!compiled || !compiled->SupportsUsage(usage))
            return std::nullopt;

        const auto& shader = compiled->GetShader(usage);
        if (!shader)
            return std::nullopt;

        return SProgramKey{ .Identity = shader.get() };
    }

    EMaterialUsage Renderer::GetUsage(const EMaterialPass pass)
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

    uint32_t Renderer::GetPassOrder(const EMaterialPass pass)
    {
        switch (pass)
        {
            case EMaterialPass::ParticleSprite: return 2;
            case EMaterialPass::ParticleRibbon: return 1;
            case EMaterialPass::ParticleMesh:   return 0;
        }

        return UINT32_MAX;
    }

    Ref<GraphicsPipeline> Renderer::GetPipeline(
        const EMaterialPass pass,
        const Ref<Shader>& shader,
        const SPipelineRequest& request
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

    bool Renderer::BindDescriptorResources(
        const Ref<Shader>& shader,
        const SPassRequest& request
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

            if (shader->HasBinding("materials"))
                shader->BindStorageBuffer("materials", request.MaterialBuffer);

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

        if (shader->HasBinding("materials"))
            shader->BindStorageBuffer("materials", request.MaterialBuffer);
        if (shader->HasBinding("sprites"))
            shader->BindTextureSet("sprites", m_Textures.GetTextureSet());
        if (shader->HasBinding("spriteSampler"))
            shader->BindSampler("spriteSampler", m_Textures.GetSampler());

        m_DescriptorBindings.emplace(shader.get(), std::move(state));
        return true;
    }
}
