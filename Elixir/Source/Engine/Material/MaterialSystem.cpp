#include "epch.h"
#include "MaterialSystem.h"

namespace Elixir
{
    namespace
    {
        struct SMaterialBatchKey
        {
            EMaterialPass Pass = EMaterialPass::ParticleSprite;
            uint32_t GeometryIndex = UINT32_MAX;
            SMaterialProgramKey Program;

            bool operator==(const SMaterialBatchKey&) const = default;
        };

        struct SMaterialBatchItem
        {
            const SMaterialRenderItem* Item = nullptr;
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
        m_FrameBuffer(DynamicStorageBuffer::Create(
            context,
            sizeof(SMaterialFrameData) * m_MaterialCapacity)
        ),
        m_Textures(context),
        m_Renderer(CreateScope<MaterialRenderer>(
            context,
            m_FrameBuffer,
            m_Textures,
            shaderLoader
        )) {}

    SMaterialFrameSnapshot MaterialSystem::BuildFrameSnapshot(
        const MaterialRenderScene& scene,
        const uint64_t submissionSerial
    )
    {
        m_Textures.BeginFrame(submissionSerial);

        const auto table = CreateRef<MaterialFrameTable>(
            m_MaterialCapacity,
            m_Textures.GetFallbackIndex(),
            [this](const Ref<Texture>& texture)
            {
                return m_Textures.Resolve(texture);
            }
        );

        for (const auto& item : scene.GetItems())
        {
            const auto& material = item.Material;
            if (material)
                table->Add(*material);
        }

        if (!table->GetData().empty())
        {
            m_FrameBuffer->UpdateData(
                table->GetData().data(),
                table->GetData().size() * sizeof(SMaterialFrameData)
            );
        }

        return { table, table->GetCount(), submissionSerial };
    }

    std::optional<SMaterialProgramKey> MaterialSystem::GetProgramKey(
        const EMaterialPass pass,
        const MaterialRenderProxy& material
    ) const
    {
        return m_Renderer->GetProgramKey(pass, material);
    }

    std::optional<SPreparedMaterialPass> MaterialSystem::PrepareMaterialPass(
        const SMaterialPassRequest& request
    ) const
    {
        return m_Renderer->Prepare(request);
    }

    SMaterialRenderResult MaterialSystem::Render(
        const Ref<CommandBuffer>& cmd,
        const MaterialRenderScene& scene,
        const SMaterialFrameSnapshot& snapshot
    ) const
    {
        if (!cmd || !snapshot.Table)
            return {};

        // Batching

        std::vector<SMaterialBatch> batches;

        for (const auto& item : scene.GetItems())
        {
            if (!item.Material)
            {
                EE_CORE_ERROR(
                    "Material render item '{}' has no compiled material proxy.",
                    item.DebugName
                )
                continue;
            }

            const auto* geometry = scene.FindGeometry(item.GeometryIndex);
            EE_CORE_ASSERT(geometry, "Material render item geometry is unavailable.")
            if (!geometry) continue;

            const auto program = GetProgramKey(item.Pass, *item.Material);
            EE_CORE_ASSERT(program, "Material render item does not support its requested pass.")
            if (!program) continue;

            const auto materialIndex = snapshot.Table->Find(*item.Material);
            EE_CORE_ASSERT(materialIndex, "Material frame snapshot is missing a render item material.")
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
                return MaterialRenderer::GetPassOrder(left.Key.Pass) <
                    MaterialRenderer::GetPassOrder(right.Key.Pass);
            }

            if (left.Key.GeometryIndex != right.Key.GeometryIndex)
                return left.Key.GeometryIndex < right.Key.GeometryIndex;

            return std::less<const void*>{}(
                left.Key.Program.Identity,
                right.Key.Program.Identity
            );
        });

        SMaterialRenderResult result{};

        for (const auto& batch : batches)
        {
            if (batch.Items.empty()) continue;

            const auto* geometry = scene.FindGeometry(batch.Key.GeometryIndex);
            if (!geometry) continue;

            const auto& first = *batch.Items.front().Item;
            const auto prepared = PrepareMaterialPass({
                .Pass = batch.Key.Pass,
                .Material = first.Material.get(),
                .Pipeline = geometry->Pipeline,
                .ExternalResources = {
                    .ConstantBuffers = geometry->ConstantBuffers,
                    .StorageBuffers = geometry->StorageBuffers,
                },
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
        return m_Renderer->Resolve(instance);
    }
}
