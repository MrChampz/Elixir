#include "epch.h"
#include "MaterialSystem.h"

#include <Engine/Materials/Rendering/FrameTable.h>

namespace Elixir::Materials
{
    namespace
    {
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
        m_FrameSlots(*context),
        m_Textures(context),
        m_ProxyResolver(shaderLoader),
        m_ProxyCache(m_ProxyResolver),
        m_Renderer(CreateScope<Renderer>(context, m_Textures)),
        m_GraphicsContext(context)
    {
        EE_CORE_ASSERT(context, "Material system requires a graphics context.")

        m_FrameSlots.ForEach([&](SFrameSlot& slot)
        {
            slot.Buffer = DynamicStorageBuffer::Create(
                context,
                sizeof(SMaterialFrameData) * m_MaterialCapacity
            );
        });
    }

    void MaterialSystem::BeginFrame()
    {
        EE_CORE_ASSERT(m_GraphicsContext, "Material system graphics context is unavailable.")

        m_CurrentFrameNumber = m_GraphicsContext->GetFrameNumber();
        m_SubmittedScenes.clear();
        m_ProxyCache.PruneExpired();
        m_Textures.BeginFrame(m_CurrentFrameNumber);
    }

    void MaterialSystem::Submit(MaterialRenderScene scene)
    {
        EE_CORE_ASSERT(
            m_CurrentFrameNumber == m_GraphicsContext->GetFrameNumber(),
            "Material scenes must be submitted after BeginFrame for the current graphics frame."
        )
        m_SubmittedScenes.push_back(std::move(scene));
    }

    SRenderResult MaterialSystem::RenderFrame()
    {
        SRenderResult result{};
        if (m_SubmittedScenes.empty()) return result;

        EE_CORE_ASSERT(
            m_CurrentFrameNumber == m_GraphicsContext->GetFrameNumber(),
            "Material rendering requires BeginFrame for the current graphics frame."
        )

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        const auto extent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        const auto renderingInfo = SRenderingInfo{
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = extent
        };
        cmd->Begin(renderingInfo);
        cmd->BeginRendering(renderingInfo);

        cmd->SetViewports({
            Viewport{
                .Width = (float)extent.Width,
                .Height = (float)extent.Height,
                .MinDepth = 0.0f,
                .MaxDepth = 1.0f,
            }
        });

        cmd->SetScissors({
            Rect2D{
                .Offset = { 0, 0 },
                .Extent = extent
            }
        });

        for (const auto& scene : m_SubmittedScenes)
        {
            const auto prepared = PrepareScene(scene);
            const auto sceneResult = m_Renderer->Record({
                .CommandBuffer = cmd,
                .Scene = &scene,
                .Items = std::span{ prepared.Items },
                .MaterialBuffer = GetActiveFrameBuffer(),
                .MaterialCount = prepared.MaterialCount,
            });

            result.MaterialCount += sceneResult.MaterialCount;
            result.BatchCount += sceneResult.BatchCount;
            result.DrawCount += sceneResult.DrawCount;
        }

        cmd->EndRendering();
        cmd->End();

        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);

        return result;
    }

    SPreparedScene MaterialSystem::PrepareScene(const MaterialRenderScene& scene)
    {
        SPreparedScene prepared{};
        const auto& frameBuffer = GetActiveFrameBuffer();

        const auto table = CreateRef<FrameTable>(
            m_MaterialCapacity,
            m_Textures.GetFallbackIndex(),
            [this](const Ref<Texture>& texture)
            {
                return m_Textures.Resolve(texture);
            }
        );

        for (const auto& item : scene.GetItems())
        {
            const auto& proxy = ResolveMaterialProxy(item.Material);
            if (!proxy) continue;

            const auto materialIndex = table->Add(*proxy);
            EE_CORE_ASSERT(materialIndex, "Material frame capacity was exceeded.")
            if (!materialIndex) continue;

            prepared.Items.push_back({
                .Item = &item,
                .Proxy = proxy,
                .MaterialIndex = *materialIndex,
            });
        }

        if (!table->GetData().empty())
        {
            frameBuffer->UpdateData(
                table->GetData().data(),
                table->GetData().size() * sizeof(SMaterialFrameData)
            );
        }

        prepared.MaterialCount = table->GetCount();
        return prepared;
    }

    Ref<const MaterialRenderProxy> MaterialSystem::ResolveMaterialProxy(
        const Ref<MaterialInstance>& instance
    )
    {
        return m_ProxyCache.Resolve(instance);
    }

    const Ref<DynamicStorageBuffer>& MaterialSystem::GetActiveFrameBuffer() const
    {
        EE_CORE_ASSERT(m_GraphicsContext, "Material system graphics context is unavailable.")
        return m_FrameSlots.GetCurrent().Buffer;
    }
}
