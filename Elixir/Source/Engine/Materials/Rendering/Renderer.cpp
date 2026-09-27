#include "epch.h"
#include "Renderer.h"

#include <Engine/Graphics/Image.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>

namespace Elixir::Materials::Rendering
{
    Renderer::Renderer(
        const GraphicsContext* context,
        const uint32_t materialCapacity
    ) : m_MaterialCapacity(materialCapacity),
        m_FrameSlots(*context),
        m_Textures(context),
        m_Context(context)
    {
        EE_CORE_ASSERT(context, "Material renderer requires a graphics context.")
        EE_CORE_ASSERT(
            m_MaterialCapacity > 0,
            "Material renderer frame capacity must be greater than zero."
        )

        m_FrameSlots.ForEach([this, context](SFrameSlot& slot)
        {
            slot.MaterialBuffer = DynamicStorageBuffer::Create(
                context,
                sizeof(SMaterialFrameData) * m_MaterialCapacity
            );
        });
    }

    void Renderer::BeginFrame()
    {
        EE_CORE_ASSERT(m_Context, "Material renderer graphics context is unavailable.")

        m_CurrentFrameNumber = m_Context->GetFrameNumber();
        m_Textures.BeginFrame(m_CurrentFrameNumber);
    }

    SRenderResult Renderer::RenderFrame(
        const std::span<const SPreparedScene> scenes,
        const Ref<Image>& sceneTarget
    )
    {
        SRenderResult result{};
        if (scenes.empty()) return result;

        EE_CORE_ASSERT(sceneTarget, "Material rendering requires a scene target.")

        EE_CORE_ASSERT(
            m_CurrentFrameNumber == m_Context->GetFrameNumber(),
            "Material rendering requires BeginFrame for the current graphics frame."
        )

        FrameTable materialTable{
            m_MaterialCapacity,
            m_Textures.GetFallbackIndex(),
            [this](const Ref<Texture>& texture)
            {
                return m_Textures.Resolve(texture);
            }
        };

        std::vector<SPreparedRenderScene> preparedScenes;
        preparedScenes.reserve(scenes.size());

        for (const auto& scene : scenes)
            preparedScenes.push_back(PrepareScene(scene, materialTable));

        if (!materialTable.GetData().empty())
        {
            GetActiveMaterialBuffer()->UpdateData(
                materialTable.GetData().data(),
                materialTable.GetData().size() * sizeof(SMaterialFrameData)
            );
        }

        result.MaterialCount = materialTable.GetCount();

        const auto cmd = m_Context->GetSecondaryCommandBuffer();
        const auto extent = sceneTarget->GetExtent();

        const SRenderingInfo renderingInfo{
            .ColorAttachment = sceneTarget,
            .DepthStencilAttachment = m_Context->GetDepthStencilRenderTarget(),
            .RenderArea = extent,
        };

        cmd->Begin(renderingInfo);
        cmd->BeginRendering(renderingInfo);

        cmd->SetViewports({{
            .Width = (float)extent.Width,
            .Height = (float)extent.Height,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        }});

        cmd->SetScissors({{
            .Offset = { 0, 0 },
            .Extent = extent,
        }});

        for (const auto& scene : preparedScenes)
        {
            const auto sceneResult = RecordScene(cmd, scene);
            result.BatchCount += sceneResult.BatchCount;
            result.DrawCount += sceneResult.DrawCount;
        }

        cmd->EndRendering();
        cmd->End();
        m_Context->EnqueueSecondaryCommandBuffer(cmd);

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
            case EMaterialPass::Surface:        return EMaterialUsage::Surface;
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
            case EMaterialPass::Surface:        return 0;
            case EMaterialPass::ParticleSprite: return 3;
            case EMaterialPass::ParticleRibbon: return 2;
            case EMaterialPass::ParticleMesh:   return 1;
        }

        return UINT32_MAX;
    }

    Renderer::SPreparedRenderScene Renderer::PrepareScene(
        const SPreparedScene& scene,
        FrameTable& materialTable
    )
    {
        SPreparedRenderScene prepared{
            .Scene = scene.Scene,
        };

        if (!scene.Scene) return prepared;

        for (const auto& resolved : scene.Items)
        {
            if (!resolved.Item || !resolved.Proxy) continue;

            const auto materialIndex = materialTable.Add(*resolved.Proxy);
            EE_CORE_ASSERT(materialIndex, "Material frame capacity was exceeded.")
            if (!materialIndex) continue;

            prepared.Items.push_back({
                .Item = resolved.Item,
                .Proxy = resolved.Proxy,
                .MaterialIndex = *materialIndex,
            });
        }

        return prepared;
    }

    SRenderResult Renderer::RecordScene(
        const Ref<CommandBuffer>& cmd,
        const SPreparedRenderScene& scene
    )
    {
        SRenderResult result{};

        if (!cmd || !scene.Scene) return result;

        std::vector<SBatch> batches;
        std::vector<const SPreparedRenderItem*> translucentItems;

        for (const auto& prepared : scene.Items)
        {
            if (!prepared.Item || !prepared.Proxy) continue;

            const auto& item = *prepared.Item;

            if (prepared.Proxy->GetBlendMode() == EMaterialBlendMode::Translucent)
            {
                translucentItems.push_back(&prepared);
                continue;
            }

            const auto* geometry = scene.Scene->FindGeometry(item.GeometryIndex);
            EE_CORE_ASSERT(geometry, "Material render item geometry is unavailable.");
            if (!geometry) continue;

            const auto program = GetProgramKey(item.Pass, *prepared.Proxy);
            EE_CORE_ASSERT(program, "Material render item does not support its requested pass.")
            if (!program) continue;

            const SBatchKey key{
                .Pass = item.Pass,
                .BlendMode = prepared.Proxy->GetBlendMode(),
                .DoubleSided = prepared.Proxy->IsDoubleSided(),
                .GeometryIndex = item.GeometryIndex,
                .Indexed = item.IndexedDraw.has_value(),
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

            batch->Items.push_back(&prepared);
        }

        std::ranges::stable_sort(
            batches,
            [](const SBatch& left, const SBatch& right)
            {
                if (left.Key.Pass != right.Key.Pass)
                    return GetPassOrder(left.Key.Pass) < GetPassOrder(right.Key.Pass);

                if (left.Key.BlendMode != right.Key.BlendMode)
                    return left.Key.BlendMode < right.Key.BlendMode;

                if (left.Key.GeometryIndex != right.Key.GeometryIndex)
                    return left.Key.GeometryIndex < right.Key.GeometryIndex;

                return std::less<const void*>{}(
                    left.Key.Program.Identity,
                    right.Key.Program.Identity
                );
            }
        );

        std::ranges::stable_sort(
            translucentItems,
            [](const SPreparedRenderItem* left, const SPreparedRenderItem* right)
            {
                return left->Item->SortDepth > right->Item->SortDepth;
            }
        );

        for (const auto& prepared : translucentItems)
        {
            const auto& item = *prepared->Item;
            const auto* geometry = scene.Scene->FindGeometry(item.GeometryIndex);
            EE_CORE_ASSERT(geometry, "Material render item geometry is unavailable.")
            if (!geometry) continue;

            const auto program = GetProgramKey(item.Pass, *prepared->Proxy);
            EE_CORE_ASSERT(program, "Material render item does not support its requested pass.")
            if (!program) continue;

            batches.push_back({
                .Key = {
                    .Pass = item.Pass,
                    .BlendMode = prepared->Proxy->GetBlendMode(),
                    .DoubleSided = prepared->Proxy->IsDoubleSided(),
                    .GeometryIndex = item.GeometryIndex,
                    .Indexed = item.IndexedDraw.has_value(),
                    .Program = *program,
                },
                .Items = { prepared },
            });
        }

        for (const auto& batch : batches)
        {
            if (batch.Items.empty()) continue;

            const auto* geometry = scene.Scene->FindGeometry(batch.Key.GeometryIndex);
            if (!geometry) continue;

            const auto& first = *batch.Items.front();
            const auto prepared = PreparePass({
                .Pass = batch.Key.Pass,
                .Material = first.Proxy.get(),
                .Pipeline = geometry->Pipeline,
                .ExternalResources = {
                    .ConstantBuffers = geometry->ConstantBuffers,
                    .StorageBuffers = geometry->StorageBuffers,
                    .Textures = geometry->Textures,
                    .Samplers = geometry->Samplers,
                },
                .MaterialBuffer = GetActiveMaterialBuffer(),
                .InitialPushConstants = std::span{
                    first.Item->PushConstants.Data.data(),
                    first.Item->PushConstants.Size
                },
            });
            if (!prepared) continue;

            ++result.BatchCount;
            prepared->Pipeline->Bind(cmd);

            for (const auto& binding : geometry->VertexBuffers)
            {
                cmd->BindBuffer<VertexBuffer>(
                    binding.Buffer,
                    std::span<uint64_t>{},
                    1,
                    binding.Binding
                );
            }

            if (batch.Key.Indexed)
            {
                cmd->BindIndexBuffer(geometry->IndexBuffer, geometry->IndexType);
            }

            for (const auto* resolved : batch.Items)
            {
                const auto constants = resolved->Item->PushConstants.Resolve(
                    resolved->MaterialIndex
                );

                prepared->Shader->SetPushConstant(
                    cmd,
                    "pc",
                    const_cast<std::byte*>(constants.data()),
                    resolved->Item->PushConstants.Size
                );

                if (const auto& indexed = resolved->Item->IndexedDraw)
                {
                    cmd->DrawIndexed(
                        indexed->IndexCount,
                        resolved->Item->Draw.InstanceCount,
                        indexed->FirstIndex,
                        indexed->VertexOffset,
                        resolved->Item->Draw.FirstInstance
                    );
                }
                else
                {
                    cmd->Draw(
                        resolved->Item->Draw.VertexCount,
                        resolved->Item->Draw.InstanceCount,
                        resolved->Item->Draw.FirstVertex,
                        resolved->Item->Draw.FirstInstance
                    );
                }

                ++result.DrawCount;
            }
        }

        return result;
    }

    std::optional<SPreparedPass> Renderer::PreparePass(const SPassRequest& request)
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
            .Pipeline = GetPipeline(
                request.Pass,
                request.Material->GetBlendMode(),
                request.Material->IsDoubleSided(),
                shader,
                request.Pipeline
            ),
        };
    }

    const Ref<DynamicStorageBuffer>& Renderer::GetActiveMaterialBuffer() const
    {
        EE_CORE_ASSERT(m_Context, "Material renderer graphics context is unavailable.")
        return m_FrameSlots.GetCurrent().MaterialBuffer;
    }

    Ref<GraphicsPipeline> Renderer::GetPipeline(
        const EMaterialPass pass,
        const EMaterialBlendMode blendMode,
        const bool doubleSided,
        const Ref<Shader>& shader,
        const SPipelineRequest& request
    )
    {
        const SPipelineKey key{
            .Pass = pass,
            .BlendMode = blendMode,
            .DoubleSided = doubleSided,
            .Shader = shader.get(),
            .VertexLayoutKey = request.VertexLayoutKey,
        };

        if (const auto found = m_Pipelines.find(key); found != m_Pipelines.end())
            return found->second;

        PipelineBuilder builder;
        builder.SetShader(shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetColorAttachmentFormat(EImageFormat::R16G16B16A16_SFLOAT);
        builder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        builder.SetBufferLayout(*request.VertexLayout);
        builder.DisableBlending();

        switch (pass)
        {
            case EMaterialPass::Surface:
            case EMaterialPass::ParticleMesh:
                builder.SetCullMode(
                    doubleSided ? ECullMode::None : ECullMode::Back,
                    EFrontFace::CounterClockwise
                );
                break;
            case EMaterialPass::ParticleSprite:
            case EMaterialPass::ParticleRibbon:
                builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
                builder.DisableDepthTest();
                break;
        }

        if (blendMode == EMaterialBlendMode::Translucent)
            builder.EnableAlphaBlending();

        auto info = builder.GetCreateInfo();

        if (pass == EMaterialPass::Surface || pass == EMaterialPass::ParticleMesh)
        {
            info.DepthStencil.DepthTestEnable = true;
            info.DepthStencil.DepthWriteEnable = blendMode != EMaterialBlendMode::Translucent;
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
                        .Type = std::is_same_v<TBuffer, Ref<StorageBuffer>>
                            ? EDescriptorBindingType::StorageBuffer
                            : EDescriptorBindingType::DynamicStorageBuffer
                    });
                },
                binding.Buffer
            );
        }

        for (const auto& binding : request.ExternalResources.Textures)
        {
            state.ExternalResources.push_back({
                .Name = std::string(binding.Name),
                .Type = EDescriptorBindingType::Texture
            });
        }

        for (const auto& binding : request.ExternalResources.Samplers)
        {
            state.ExternalResources.push_back({
                .Name = std::string(binding.Name),
                .Type = EDescriptorBindingType::Sampler
            });
        }

        const auto found = m_DescriptorBindings.find(shader.get());
        if (found != m_DescriptorBindings.end())
        {
            if (found->second != state)
            {
                EE_CORE_ERROR("Material shader descriptor bindings changed after initialization.")
                return false;
            }
        }
        else
        {
            m_DescriptorBindings.emplace(shader.get(), std::move(state));
        }

        for (const auto& binding : request.ExternalResources.ConstantBuffers)
            shader->BindConstantBuffer(binding.Name, binding.Buffer);

        for (const auto& binding : request.ExternalResources.StorageBuffers)
        {
            std::visit(
                [&shader, &binding](const auto& buffer)
                {
                    shader->BindStorageBuffer(binding.Name, buffer);
                },
                binding.Buffer
            );
        }

        for (const auto& binding : request.ExternalResources.Textures)
            shader->BindTexture(binding.Name, binding.Texture);

        for (const auto& binding : request.ExternalResources.Samplers)
            shader->BindSampler(binding.Name, binding.Sampler);

        if (shader->HasBinding("materials"))
            shader->BindStorageBuffer("materials", request.MaterialBuffer);
        if (shader->HasBinding("sprites"))
            shader->BindTextureSet("sprites", m_Textures.GetTextureSet());
        if (shader->HasBinding("spriteSampler"))
            shader->BindSampler("spriteSampler", m_Textures.GetSampler());
        if (shader->HasBinding("textures"))
            shader->BindTextureSet("textures", m_Textures.GetTextureSet());
        if (shader->HasBinding("texSampler"))
            shader->BindSampler("texSampler", m_Textures.GetSampler());

        return true;
    }
}
