#pragma once

#include <Engine/Graphics/FrameSlotState.h>

namespace Elixir::GUI
{
    /** Keeps geometry resources and their uploaded revision separate for each frame in flight. */
    template <typename TResource>
    class FrameGeometrySlots final
    {
    public:
        /** Associates geometry slots with the graphics context's frame fences. */
        explicit FrameGeometrySlots(const GraphicsContext& context) : m_Slots(context) {}

        /** Marks the CPU geometry as changed. Other frame slots catch up when reused. */
        void MarkChanged() { ++m_Revision; }

        /**
         * Updates stale geometry in the current slot and returns its resource.
         * @return The current resource, or nullptr if the upload failed.
         * @pre The graphics context has waited for the current slot's fence.
         * @param upload Returns true after successfully uploading the current geometry.
         */
        template <typename TUpload>
        TResource* UpdateCurrent(TUpload&& upload)
        {
            auto& slot = m_Slots.GetCurrent();
            if (slot.Revision != m_Revision)
            {
                if (!std::invoke(upload, slot.Resource)) return nullptr;
                slot.Revision = m_Revision;
            }
            return &slot.Resource;
        }

    private:
        struct SSlot
        {
            TResource Resource{};
            uint64_t Revision = 0;
        };

        FrameSlotState<SSlot> m_Slots;
        uint64_t m_Revision = 1;
    };
}
