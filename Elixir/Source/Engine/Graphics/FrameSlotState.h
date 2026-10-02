#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/GraphicsContext.h>

namespace Elixir
{
    /**
     * @brief Stores one value for each frame slot of a graphics context.
     *
     * The current value is selected from GraphicsContext::GetFrameIndex().
     *
     * @tparam T Value stored for each frame slot.
     */
    template <typename T>
    class FrameSlotState final
    {
    public:
        /**
         * @brief Creates values associated with a graphics context.
         * @param context Graphics context that owns frame slots.
         */
        explicit FrameSlotState(const GraphicsContext& context)
          : m_GraphicsContext(context) {}

        /**
         * @brief Returns the index of the current frame slot.
         * @return The index of the current frame slot.
         */
        uint32_t GetCurrentFrameIndex() const
        {
            return m_GraphicsContext.GetFrameIndex();
        }

        /**
         * @brief Returns the value for the current frame slot.
         * @return The value stored for the currently active frame slot.
         */
        T& GetCurrent()
        {
            return m_Values[GetCurrentFrameIndex()];
        }

        /**
         * @brief Returns the value for the current frame slot.
         * @return The value stored for the currently active frame slot.
         */
        const T& GetCurrent() const
        {
            return m_Values[GetCurrentFrameIndex()];
        }

        /**
         * @brief Returns the value for a frame slot.
         * @param frameIndex Frame slot index.
         * @return The value stored for @p frameIndex frame slot.
         */
        T& Get(const uint32_t frameIndex)
        {
            ValidateFrameIndex(frameIndex);
            return m_Values[frameIndex];
        }

        /**
         * @brief Returns the value for a frame slot.
         * @param frameIndex Frame slot index.
         * @return The value stored for @p frameIndex frame slot.
         */
        const T& Get(const uint32_t frameIndex) const
        {
            ValidateFrameIndex(frameIndex);
            return m_Values[frameIndex];
        }

        /**
         * @brief Invokes a function for every frame-slot value.
         * @tparam TFunction The function type.
         * @param function Function invoked for each value.
         */
        template <typename TFunction>
        void ForEach(TFunction&& function)
        {
            for (auto& value : m_Values)
                std::invoke(std::forward<TFunction>(function), value);
        }

    private:
        static void ValidateFrameIndex(const uint32_t frameIndex)
        {
            EE_CORE_ASSERT(
                frameIndex < GraphicsContext::FRAMES,
                "Frame slot index is out of range."
            )
        }

        std::array<T, GraphicsContext::FRAMES> m_Values;
        const GraphicsContext& m_GraphicsContext;
    };
}
