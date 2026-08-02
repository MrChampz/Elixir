#include "epch.h"
#include "MaterialSystem.h"

namespace Elixir
{
    MaterialSystem::MaterialSystem(const GraphicsContext* context, const uint32_t capacity)
      : m_MaterialCapacity(capacity),
        m_FrameBuffer(DynamicStorageBuffer::Create(
            context,
            sizeof(SMaterialFrameData) * capacity)
        ),
        m_Textures(context),
        m_Renderer(CreateScope<MaterialRenderer>(
            context,
            m_FrameBuffer,
            m_Textures
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

            if (item.AdditionalTexture)
                m_Textures.Resolve(item.AdditionalTexture);
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

    uint32_t MaterialSystem::FindTextureIndex(const Ref<Texture>& texture) const
    {
        return m_Textures.Find(texture);
    }
}
