#include "epch.h"
#include "MaterialSystem.h"
#include "ParticleMaterialDefaults.h"

#include <Engine/Material/MaterialCompiler.h>
#include <Engine/Material/MaterialInstance.h>

namespace Elixir
{
    MaterialSystem::MaterialSystem(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const uint32_t capacity
    ) : m_MaterialCapacity(capacity),
        m_FrameBuffer(DynamicStorageBuffer::Create(
            context,
            sizeof(SMaterialFrameData) * capacity)
        ),
        m_Textures(context),
        m_Renderer(CreateScope<MaterialRenderer>(
            context,
            m_FrameBuffer,
            m_Textures
        ))
    {
        CreateDefaultParticleMaterials(shaderLoader);
    }

    SMaterialFrameSnapshot MaterialSystem::BuildFrameSnapshot(
        const std::span<const Ref<const MaterialRenderProxy>> materials,
        const std::span<const Ref<Texture>> textures,
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

        for (const auto& material : materials)
        {
            if (material)
                table->Add(*material);
        }

        for (const auto& texture : textures)
            m_Textures.Resolve(texture);

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

    const Ref<const MaterialRenderProxy>& MaterialSystem::ResolveParticleMaterial(
        const EMaterialPass pass,
        const Ref<const MaterialRenderProxy>& authoredMaterial
    ) const
    {
        if (authoredMaterial && m_Renderer->GetProgramKey(pass, *authoredMaterial))
            return authoredMaterial;

        const auto& fallback = m_DefaultParticleMaterials[GetParticleMaterialSlot(pass)];
        EE_CORE_ASSERT(fallback, "MaterialSystem default particle material is unavailable.")

        return fallback;
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

    void MaterialSystem::CreateDefaultParticleMaterials(const ShaderLoader* shaderLoader)
    {
        for (const auto pass : {
            EMaterialPass::ParticleSprite,
            EMaterialPass::ParticleRibbon,
            EMaterialPass::ParticleMesh,
        })
        {
            const auto source = CreateDefaultParticleMaterial(MaterialRenderer::GetUsage(pass));
            const auto compiled = MaterialCompiler::Compile(shaderLoader, *source);

            EE_CORE_ASSERT(
                compiled,
                "Default particle material compilation failed: {}",
                compiled.Diagnostics
            )
            if (!compiled) continue;

            const auto instance = CreateRef<MaterialInstance>(source);
            const auto proxy = instance->CreateRenderProxy(compiled.Material);

            EE_CORE_ASSERT(
                proxy,
                "Default particle material render proxy creation failed."
            )

            m_DefaultParticleMaterials[GetParticleMaterialSlot(pass)] = proxy;
        }
    }
}
