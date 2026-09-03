#pragma once

#include <array>
#include <concepts>
#include <functional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/Core.h>
#include <Engine/Graphics/GraphicsContext.h>

namespace Elixir
{
    /**
     * @brief Stores a resource for each frame slot and applies pending keyed state.
     *
     * The associated graphics context selects the current frame slot. Each key
     * keeps its desired value and the revision applied to every frame slot.
     *
     * @tparam TResource Resource stored for each frame slot.
     * @tparam TKey Key that identifies an independently tracked state value.
     * @tparam TValue Desired value associated with a key.
     */
    template <typename TResource, typename TKey, typename TValue>
    class FrameSlotState final
    {
    public:
        /**
         * @brief Describes one value pending application to a frame resource.
         *
         * References remain valid only for the duration of ApplyPendingState.
         */
        struct SPendingState
        {
            const TKey& Key;
            const TValue& Value;
        };

        /**
         * @brief Creates frame resources associated with a graphics context.
         * @param context Context that owns the frame slots.
         */
        explicit FrameSlotState(const GraphicsContext& context)
            : m_Context(context) {}

        /**
         * @brief Stores a desired value when it differs from the current value.
         * @param key The key.
         * @param value The value.
         * @return True when the value changed and must be applied to frame slots.
         */
        bool Set(const TKey& key, TValue value)
        {
            const auto found = m_States.find(key);

            if (found == m_States.end())
            {
                m_States.emplace(key, SState{
                    .Value = std::move(value),
                    .Revision = 1
                });
                return true;
            }

            auto& state = found->second;
            if (state.Value == value)
                return false;

            state.Value = std::move(value);
            ++state.Revision;
            return true;
        }

        /**
         * @brief Applies state not yet materialized in the current frame slot.
         *
         * The callback receives the resource for GraphicsContext::GetFrameIndex()
         * and every key whose desired revision differs from that slot's revision.
         * Revisions are recorded only after the callback returns.
         *
         * @tparam TApply The callback type.
         * @param apply Receives TResource& and std::span<const SPendingState>.
         */
        template <typename TApply>
        requires std::invocable<TApply, TResource&, std::span<const SPendingState>>
        void ApplyPendingState(TApply&& apply)
        {
            const auto frameIndex = m_Context.GetFrameIndex();
            EE_CORE_ASSERT(frameIndex < FRAMES, "Frame slot index is out of range.")

            std::vector<std::reference_wrapper<SState>> pendingStates;
            std::vector<SPendingState> pendingValues;

            for (auto& [key, state] : m_States)
            {
                if (state.AppliedRevisions[frameIndex] == state.Revision)
                    continue;

                pendingStates.emplace_back(state);
                pendingValues.emplace_back(key, state.Value);
            }

            if (pendingValues.empty())
                return;

            std::invoke(
                std::forward<TApply>(apply),
                m_Resources[frameIndex],
                std::span<const SPendingState>(pendingValues)
            );

            for (auto& state : pendingStates)
                state.get().AppliedRevisions[frameIndex] = state.get().Revision;
        }

        /** @brief Returns the resource for the current frame slot. */
        TResource& GetCurrentFrameResource()
        {
            return m_Resources[m_Context.GetFrameIndex()];
        }

        /** @brief Returns the resource for the current frame slot. */
        const TResource& GetCurrentFrameResource() const
        {
            return m_Resources[m_Context.GetFrameIndex()];
        }

        /** @brief Returns the resource for a specific frame slot. */
        TResource& GetResource(const uint32_t frameIndex)
        {
            EE_CORE_ASSERT(frameIndex < FRAMES, "Frame slot index is out of range.")
            return m_Resources[frameIndex];
        }

        /** @brief Returns the resource for a specific frame slot. */
        const TResource& GetResource(const uint32_t frameIndex) const
        {
            EE_CORE_ASSERT(frameIndex < FRAMES, "Frame slot index is out of range.")
            return m_Resources[frameIndex];
        }

    private:
        static constexpr uint32_t FRAMES = GraphicsContext::FRAMES;

        struct SState
        {
            TValue Value;
            uint64_t Revision = 0;
            std::array<uint64_t, FRAMES> AppliedRevisions{};
        };

        const GraphicsContext& m_Context;
        std::array<TResource, FRAMES> m_Resources;
        std::unordered_map<TKey, SState> m_States;
    };
}
