#include "epch.h"
#include "MaterialSystem.h"

namespace Elixir::Materials
{
    MaterialSystem::MaterialSystem(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const SMaterialSystemConfig config
    ) : m_ProxyResolver(shaderLoader),
        m_ProxyCache(m_ProxyResolver),
        m_Renderer(CreateScope<Renderer>(context, config.InitialFrameCapacity)) {}

    void MaterialSystem::BeginFrame()
    {
        m_SubmittedScenes.clear();
        m_ProxyCache.PruneExpired();
        m_Renderer->BeginFrame();
        m_IsCollectingFrame = true;
    }

    void MaterialSystem::Submit(MaterialRenderScene scene)
    {
        EE_CORE_ASSERT(
            m_IsCollectingFrame,
            "Material scenes must be submitted after BeginFrame."
        )
        m_SubmittedScenes.push_back(std::move(scene));
    }

    SRenderResult MaterialSystem::RenderFrame()
    {
        EE_CORE_ASSERT(m_IsCollectingFrame, "Material rendering requires BeginFrame.")

        std::vector<SPreparedScene> scenes;
        scenes.reserve(m_SubmittedScenes.size());

        for (const auto& scene : m_SubmittedScenes)
            scenes.push_back(PrepareScene(scene));

        m_IsCollectingFrame = false;
        return m_Renderer->RenderFrame(scenes);
    }

    SPreparedScene MaterialSystem::PrepareScene(const MaterialRenderScene& scene)
    {
        SPreparedScene prepared{
            .Scene = &scene,
        };

        for (const auto& item : scene.GetItems())
        {
            const auto& proxy = ResolveMaterialProxy(item.Material);
            if (!proxy) continue;

            prepared.Items.push_back({
                .Item = &item,
                .Proxy = proxy,
            });
        }

        return prepared;
    }

    Ref<const MaterialRenderProxy> MaterialSystem::ResolveMaterialProxy(
        const Ref<MaterialInstance>& instance
    )
    {
        return m_ProxyCache.Resolve(instance);
    }
}
