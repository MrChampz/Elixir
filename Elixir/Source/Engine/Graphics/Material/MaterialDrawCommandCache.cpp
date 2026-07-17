#include "epch.h"
#include "MaterialDrawCommandCache.h"

namespace Elixir
{
    MaterialDrawCommandCache::SUpdate MaterialDrawCommandCache::Update(SlotList slots)
    {
        SUpdate update;
        update.LayoutChanged = slots.size() != m_Slots.size();
        update.DirtySlots.reserve(slots.size());
        for (uint32_t slot = 0; slot < slots.size(); ++slot)
        {
            if (slot >= m_Slots.size()
                || slots[slot].Resource != m_Slots[slot].Resource
                || slots[slot].BindingRevision != m_Slots[slot].BindingRevision
                || slots[slot].BlendPass != m_Slots[slot].BlendPass)
            {
                update.DirtySlots.push_back(slot);
            }
        }
        m_Slots = std::move(slots);
        return update;
    }
}
