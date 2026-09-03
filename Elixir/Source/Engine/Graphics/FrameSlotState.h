#pragma once

#include <array>

#include <Engine/Core/Core.h>

namespace Elixir
{
    /**
     * @brief Stores one value and synchronization state for each frame slot.
     *
     * The owner selects the slot that is safe to update after the graphics
     * context has waited for its fence.
     *
     * @tparam T Value stored for each frame slot.
     * @tparam FrameCount Number of frame slots.
     */
    template <typename T, uint32_t FrameCount>
    class FrameSlotState final
    {
    public:
        /** @brief Creates frame slots with default-constructed values. */
        FrameSlotState()
        {
            m_Dirty.fill(true);
        }

        /**
         * @brief Selects the frame slot used by active-slot operations.
         * @param frameIndex Frame slot that is safe to update.
         */
        void SetActiveFrameSlot(const uint32_t frameIndex)
        {
            ValidateFrameSlot(frameIndex);
            m_ActiveFrameSlot = frameIndex;
        }

        /** @brief Returns the selected frame slot index. */
        uint32_t GetActiveFrameSlot() const { return m_ActiveFrameSlot; }

        /** @brief Returns the value stored for the selected frame slot. */
        T& GetActive() { return m_Values[m_ActiveFrameSlot]; }

        /** @brief Returns the value stored for the selected frame slot. */
        const T& GetActive() const { return m_Values[m_ActiveFrameSlot]; }

        /**
         * @brief Returns the value stored for a frame slot.
         * @param frameIndex Frame slot to access.
         */
        T& Get(const uint32_t frameIndex)
        {
            ValidateFrameSlot(frameIndex);
            return m_Values[frameIndex];
        }

        /**
         * @brief Returns the value stored for a frame slot.
         * @param frameIndex Frame slot to access.
         */
        const T& Get(const uint32_t frameIndex) const
        {
            ValidateFrameSlot(frameIndex);
            return m_Values[frameIndex];
        }

        /** @brief Marks all frame slots as needing synchronization. */
        void MarkDirty() { m_Dirty.fill(true); }

        /** @brief Marks the selected frame slot as needing synchronization. */
        void MarkActiveDirty() { m_Dirty[m_ActiveFrameSlot] = true; }

        /**
         * @brief Marks a frame slot as needing synchronization.
         * @param frameIndex Frame slot to invalidate.
         */
        void MarkDirty(const uint32_t frameIndex)
        {
            ValidateFrameSlot(frameIndex);
            m_Dirty[frameIndex] = true;
        }

        /** @brief Checks whether the selected frame slot needs synchronization. */
        bool IsActiveDirty() const { return m_Dirty[m_ActiveFrameSlot]; }

        /**
         * @brief Checks whether a frame slot needs synchronization.
         * @param frameIndex Frame slot to inspect.
         */
        bool IsDirty(const uint32_t frameIndex) const
        {
            ValidateFrameSlot(frameIndex);
            return m_Dirty[frameIndex];
        }

        /** @brief Marks the selected frame slot as synchronized. */
        void MarkActiveClean() { m_Dirty[m_ActiveFrameSlot] = false; }

        /**
         * @brief Marks a frame slot as synchronized.
         * @param frameIndex Frame slot to mark clean.
         */
        void MarkClean(const uint32_t frameIndex)
        {
            ValidateFrameSlot(frameIndex);
            m_Dirty[frameIndex] = false;
        }

    private:
        // Validates a frame-slot index before accessing internal storage.
        static void ValidateFrameSlot(const uint32_t frameIndex)
        {
            EE_CORE_ASSERT(frameIndex < FrameCount, "Frame slot index is out of range.")
        }

        std::array<T, FrameCount> m_Values;
        std::array<bool, FrameCount> m_Dirty{};
        uint32_t m_ActiveFrameSlot = 0;
    };
}
