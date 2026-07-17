#pragma once

#include <Engine/Graphics/MeshPassProcessor.h>
#include <Engine/Graphics/Material/MaterialResource.h>

#include <cstdint>
#include <vector>

namespace Elixir
{
    // Tracks the immutable inputs used to build the draw commands for each material
    // slot. Runtime parameter edits remain cache hits unless they change pass
    // classification; resources and binding revisions select shaders and pipelines.
    class ELIXIR_API MaterialDrawCommandCache
    {
      public:
        struct SSlot
        {
            Ref<const MaterialResource> Resource;
            uint64_t BindingRevision = 0;
            EMeshPass Pass = EMeshPass::BaseOpaque;
        };

        using SlotList = std::vector<SSlot>;

        struct SUpdate
        {
            bool LayoutChanged = false;
            std::vector<uint32_t> DirtySlots;

            [[nodiscard]] bool HasChanges() const
            {
                return LayoutChanged || !DirtySlots.empty();
            }
        };

        SUpdate Update(SlotList slots);
        void Clear() { m_Slots.clear(); }

        [[nodiscard]] const SlotList& GetSlots() const { return m_Slots; }

      private:
        SlotList m_Slots;
    };
}
