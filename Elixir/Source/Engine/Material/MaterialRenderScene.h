#pragma once

#include <Engine/Material/MaterialRenderer.h>

namespace Elixir
{
    // A frame-local material item. Geometry producers may temporarily expose
    // an additional texture while their legacy data is migrated into MaterialRenderProxy.
    struct SMaterialRenderItem
    {
        EMaterialPass Pass = EMaterialPass::ParticleSprite;
        Ref<const MaterialRenderProxy> Material;
        Ref<Texture> AdditionalTexture;
    };

    // Immutable after frame publication. MaterialSystem consumes this object
    // synchronously while recording the frame command buffer.
    class ELIXIR_API MaterialRenderScene final
    {
    public:
        void Add(SMaterialRenderItem item);

        std::span<const SMaterialRenderItem> GetItems() const { return m_Items; }

    private:
        std::vector<SMaterialRenderItem> m_Items;
    };
}