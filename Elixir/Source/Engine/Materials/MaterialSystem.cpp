#include "epch.h"
#include "MaterialSystem.h"

namespace Elixir::Materials
{
    namespace
    {
        struct SMaterialBatchKey
        {
            EMaterialPass Pass = EMaterialPass::ParticleSprite;
            uint32_t GeometryIndex = UINT32_MAX;
            SProgramKey Program;

            bool operator==(const SMaterialBatchKey&) const = default;
        };

        struct SMaterialBatchItem
        {
            const SRenderItem* Item = nullptr;
            uint32_t MaterialIndex = UINT32_MAX;
        };

        struct SMaterialBatch
        {
            SMaterialBatchKey Key;
            std::vector<SMaterialBatchItem> Items;
        };

        uint32_t GetInitialFrameCapacity(const SMaterialSystemConfig config)
        {
            EE_CORE_ASSERT(
                config.InitialFrameCapacity != 0,
                "Material system frame capacity must be greater than zero."
            )
            return config.InitialFrameCapacity;
        }
    }

    MaterialSystem::MaterialSystem(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const SMaterialSystemConfig config
    ) : m_MaterialCapacity(GetInitialFrameCapacity(config)),
        m_Textures(context),
        m_Renderer(CreateScope<Renderer>(
            context,
            m_Textures,
            shaderLoader
        )),
        m_Context(context)
    {
        EE_CORE_ASSERT(context, "Material system requires a graphics context.")

        for (auto& slot : m_FrameSlots)
        {
            slot.Buffer = DynamicStorageBuffer::Create(
                context,
                sizeof(SMaterialFrameData) * m_MaterialCapacity
            );
        }
    }

    void MaterialSystem::BeginFrame()
    {
        EE_CORE_ASSERT(m_Context, "Material system graphics context is unavailable.")

        m_CurrentFrameNumber = m_Context->GetFrameNumber();
        m_SubmittedScenes.clear();
        m_Textures.BeginFrame(m_CurrentFrameNumber);

        auto& slot = m_FrameSlots[m_Context->GetFrameIndex()];
        slot.Table.reset();
        slot.FrameNumber = m_CurrentFrameNumber;
        m_PreparedFrame = {};
    }

    void MaterialSystem::Submit(MaterialRenderScene scene)
    {
        EE_CORE_ASSERT(
            m_CurrentFrameNumber == m_Context->GetFrameNumber(),
            "Material scenes must be submitted after BeginFrame for the current graphics frame."
        )
        m_SubmittedScenes.push_back(std::move(scene));
    }

    SMaterialRenderResult MaterialSystem::RenderFrame()
    {
        SMaterialRenderResult result{};
        if (m_SubmittedScenes.empty()) return result;

        EE_CORE_ASSERT(
            m_CurrentFrameNumber == m_Context->GetFrameNumber(),
            "Material rendering requires BeginFrame for the current graphics frame."
        )

        PrepareScenes(m_SubmittedScenes, m_CurrentFrameNumber);

        const auto cmd = m_Context->GetSecondaryCommandBuffer();
        cmd->Begin({
            .ColorAttachment = m_Context->GetRenderTarget(),
            .DepthStencilAttachment = m_Context->GetDepthStencilRenderTarget(),
            .RenderArea = m_Context->GetRenderTarget()->GetExtent(),
        });

        const auto extent = m_Context->GetRenderTarget()->GetExtent();
        cmd->BeginRendering({
            .ColorAttachment = m_Context->GetRenderTarget(),
            .DepthStencilAttachment = m_Context->GetDepthStencilRenderTarget(),
            .RenderArea = extent,
        });
        cmd->SetViewports({ Viewport{ .Width = (float)extent.Width, .Height = (float)extent.Height, .MinDepth = 0.0f, .MaxDepth = 1.0f } });
        cmd->SetScissors({ Rect2D{ .Offset = { 0, 0 }, .Extent = extent } });

        for (const auto& scene : m_SubmittedScenes)
        {
            const auto sceneResult = Render(cmd, scene, m_CurrentFrameNumber);
            result.MaterialCount += sceneResult.MaterialCount;
            result.BatchCount += sceneResult.BatchCount;
            result.DrawCount += sceneResult.DrawCount;
        }

        cmd->EndRendering();
        cmd->End();
        m_Context->EnqueueSecondaryCommandBuffer(cmd);
        return result;
    }

    void MaterialSystem::PrepareFrame(
        const MaterialRenderScene& scene,
        const uint64_t submissionSerial
    )
    {
        PrepareScenes(std::span<const MaterialRenderScene>{ &scene, 1 }, submissionSerial);
    }

    void MaterialSystem::PrepareScenes(
        const std::span<const MaterialRenderScene> scenes,
        const uint64_t submissionSerial
    )
    {
        const auto& frameBuffer = GetFrameBuffer();

        const auto table = CreateRef<FrameTable>(
            m_MaterialCapacity,
            m_Textures.GetFallbackIndex(),
            [this](const Ref<Texture>& texture)
            {
                return m_Textures.Resolve(texture);
            }
        );

        for (const auto& scene : scenes)
        {
            for (const auto& item : scene.GetItems())
            {
                const auto& material = item.Material;
                if (!material) continue;

                const auto proxy = Resolve(material);
                if (proxy) table->Add(*proxy);
            }
        }

        if (!table->GetData().empty())
        {
            frameBuffer->UpdateData(
                table->GetData().data(),
                table->GetData().size() * sizeof(SMaterialFrameData)
            );
        }

        m_PreparedFrame = {
            .Table = table,
            .MaterialCount = table->GetCount(),
            .SubmissionSerial = submissionSerial,
        };

        auto& slot = m_FrameSlots[m_Context->GetFrameIndex()];
        slot.Table = table;
        slot.FrameNumber = submissionSerial;
    }

    std::optional<SProgramKey> MaterialSystem::GetProgramKey(
        const EMaterialPass pass,
        const MaterialRenderProxy& material
    ) const
    {
        return m_Renderer->GetProgramKey(pass, material);
    }

    std::optional<SPreparedPass> MaterialSystem::PrepareMaterialPass(
        const SPassRequest& request
    ) const
    {
        return m_Renderer->Prepare(request);
    }

    SMaterialRenderResult MaterialSystem::Render(
        const Ref<CommandBuffer>& cmd,
        const MaterialRenderScene& scene,
        const uint64_t submissionSerial
    )
    {
        const auto isPrepared = m_PreparedFrame.Table &&
            m_PreparedFrame.SubmissionSerial == submissionSerial;

        EE_CORE_ASSERT(isPrepared, "Material rendering requires a prepared frame for the submission.")

        SMaterialRenderResult result{
            .MaterialCount = m_PreparedFrame.MaterialCount,
        };

        if (!cmd || !isPrepared) return result;

        const auto& table = m_PreparedFrame.Table;

        // Batching

        std::vector<SMaterialBatch> batches;

        for (const auto& item : scene.GetItems())
        {
            if (!item.Material)
            {
                EE_CORE_ERROR("Material render item has no compiled material proxy.")
                continue;
            }

            const auto* geometry = scene.FindGeometry(item.GeometryIndex);
            EE_CORE_ASSERT(geometry, "Material render item geometry is unavailable.")
            if (!geometry) continue;

            const auto material = Resolve(item.Material);
            if (!material) continue;

            const auto program = GetProgramKey(item.Pass, *material);
            EE_CORE_ASSERT(program, "Material render item does not support its requested pass.")
            if (!program) continue;

            const auto materialIndex = table->Find(*material);
            EE_CORE_ASSERT(materialIndex, "Prepared material frame is missing a render item material.")
            if (!materialIndex) continue;

            const SMaterialBatchKey key{
                .Pass = item.Pass,
                .GeometryIndex = item.GeometryIndex,
                .Program = *program,
            };

            auto batch = std::ranges::find_if(batches, [&key](const SMaterialBatch& candidate)
            {
                return candidate.Key == key;
            });

            if (batch == batches.end())
            {
                batches.push_back({ .Key = key });
                batch = std::prev(batches.end());
            }

            batch->Items.push_back({
                .Item = &item,
                .MaterialIndex = *materialIndex,
            });
        }

        std::ranges::stable_sort(batches, [](const SMaterialBatch& left, const SMaterialBatch& right)
        {
            if (left.Key.Pass != right.Key.Pass)
            {
                return Renderer::GetPassOrder(left.Key.Pass) <
                    Renderer::GetPassOrder(right.Key.Pass);
            }

            if (left.Key.GeometryIndex != right.Key.GeometryIndex)
                return left.Key.GeometryIndex < right.Key.GeometryIndex;

            return std::less<const void*>{}(
                left.Key.Program.Identity,
                right.Key.Program.Identity
            );
        });

        for (const auto& batch : batches)
        {
            if (batch.Items.empty()) continue;

            const auto* geometry = scene.FindGeometry(batch.Key.GeometryIndex);
            if (!geometry) continue;

            const auto& first = *batch.Items.front().Item;
            const auto prepared = PrepareMaterialPass({
                .Pass = batch.Key.Pass,
                .Material = Resolve(first.Material).get(),
                .Pipeline = geometry->Pipeline,
                .ExternalResources = {
                    .ConstantBuffers = geometry->ConstantBuffers,
                    .StorageBuffers = geometry->StorageBuffers,
                },
                .MaterialBuffer = GetFrameBuffer(),
                .InitialPushConstants = std::span{
                    first.PushConstants.Data.data(),
                    first.PushConstants.Size,
                },
            });
            if (!prepared) continue;

            ++result.BatchCount;

            // Drawing

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

            for (const auto& batchItem : batch.Items)
            {
                const auto& item = *batchItem.Item;
                const auto constants = item.PushConstants.Resolve(batchItem.MaterialIndex);

                prepared->Shader->SetPushConstant(
                    cmd,
                    "pc",
                    const_cast<std::byte*>(constants.data()),
                    item.PushConstants.Size
                );

                cmd->Draw(
                    item.Draw.VertexCount,
                    item.Draw.InstanceCount,
                    item.Draw.FirstVertex,
                    item.Draw.FirstInstance
                );

                ++result.DrawCount;
            }
        }

        return result;
    }

    Ref<const MaterialRenderProxy> MaterialSystem::Resolve(
        const Ref<MaterialInstance>& instance
    )
    {
        if (!instance || !instance->GetParent()) return nullptr;

        const auto materialRevision = instance->GetParent()->GetRevision();
        if (const auto found = m_MaterialCache.find(instance.get());
            found != m_MaterialCache.end() &&
            found->second.InstanceRevision == instance->GetRevision() &&
            found->second.MaterialRevision == materialRevision)
        {
            return found->second.Proxy;
        }

        const auto proxy = m_Renderer->Resolve(instance);
        m_MaterialCache.insert_or_assign(instance.get(), SCachedMaterial{
            .Proxy = proxy,
            .InstanceRevision = instance->GetRevision(),
            .MaterialRevision = materialRevision,
        });
        return proxy;
    }

    const Ref<DynamicStorageBuffer>& MaterialSystem::GetFrameBuffer() const
    {
        EE_CORE_ASSERT(m_Context, "Material system graphics context is unavailable.")
        return m_FrameSlots[m_Context->GetFrameIndex()].Buffer;
    }
}
