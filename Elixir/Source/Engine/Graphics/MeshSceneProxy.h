#pragma once

#include <Engine/Graphics/Model.h>

namespace Elixir
{
    // Lifetime token owned by Model. Proxies only observe it, so renderer-side state
    // can detect a released authoring object without reading or retaining that object.
    struct SMeshSceneLifetime
    {
    };

    // Immutable render-thread view of one model. Geometry snapshots are shared across
    // material-only updates, while each proxy captures one published material list.
    class ELIXIR_API MeshSceneProxy
    {
      public:
        using PrimitiveList = std::vector<SModelPrimitive>;
        using MaterialList = Model::MaterialRenderProxyList;

        MeshSceneProxy(SModelRenderHandle handle,
            Ref<const PrimitiveList> primitives,
            Ref<const MaterialList> materials,
            Weak<SMeshSceneLifetime> lifetime)
            : m_Handle(handle),
              m_Primitives(std::move(primitives)),
              m_Materials(std::move(materials)),
              m_Lifetime(std::move(lifetime))
        {
        }

        [[nodiscard]] SModelRenderHandle GetHandle() const { return m_Handle; }
        [[nodiscard]] const Ref<const PrimitiveList>& GetPrimitiveSnapshot() const
        {
            return m_Primitives;
        }
        [[nodiscard]] const PrimitiveList& GetPrimitives() const
        {
            static const PrimitiveList empty;
            return m_Primitives ? *m_Primitives : empty;
        }
        [[nodiscard]] const Ref<const MaterialList>& GetMaterials() const
        {
            return m_Materials;
        }
        [[nodiscard]] bool IsOwnerAlive() const { return !m_Lifetime.expired(); }

      private:
        SModelRenderHandle m_Handle;
        Ref<const PrimitiveList> m_Primitives;
        Ref<const MaterialList> m_Materials;
        Weak<SMeshSceneLifetime> m_Lifetime;
    };
}
