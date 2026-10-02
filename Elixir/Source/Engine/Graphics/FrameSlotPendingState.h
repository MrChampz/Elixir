#pragma once

#include <concepts>
#include <functional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/Core.h>
#include <Engine/Graphics/FrameSlotState.h>

namespace Elixir
{
    /**
     * @brief Applies keyed state changes to the resource for each frame slot.
     *
     * Each key has a desired value and a revision. A revision is materialized
     * once for each frame slot, when that slot becomes current.
     *
     * @tparam TResource Resource stored for each frame slot.
     * @tparam TKey Key that identifies an independently tracked value.
     * @tparam TValue Value associated with a key.
     */
    template <typename TResource, typename TKey, typename TValue>
    class FrameSlotPendingState final
    {
    public:
        /**
         * @brief Describes one value pending materialization.
         *
         * References remain valid only during ApplyPendingState.
         */
        struct SPendingState
        {
            const TKey& Key;
            const TValue& Value;
        };

        /**
         * @brief Creates resources associated with a graphics context.
         * @param context Context that owns the frame slots.
         */
        explicit FrameSlotPendingState(const GraphicsContext& context)
          : m_Resources(context) {}

        /**
         * @brief Stores a value when it differs from the current desired value.
         * @param key Key that identifies the value.
         * @param value Desired value.
         * @return True when the desired value changed.
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
            const auto frameIndex = m_Resources.GetCurrentFrameIndex();
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
                m_Resources.GetCurrent(),
                std::span<const SPendingState>(pendingValues)
            );

            for (auto& state : pendingStates)
                state.get().AppliedRevisions[frameIndex] = state.get().Revision;
        }

        /**
         * @brief Invokes a function for every frame-slot resource.
         * @tparam TFunction The function type.
         * @param function Function invoked for each resource.
         */
        template <typename TFunction>
        void ForEach(TFunction&& function)
        {
            m_Resources.ForEach(std::forward<TFunction>(function));
        }

        /** @brief Returns the resource for the current frame slot. */
        TResource& GetCurrent() { return m_Resources.GetCurrent(); }

        /** @brief Returns the resource for the current frame slot. */
        const TResource& GetCurrent() const { return m_Resources.GetCurrent(); }

    private:
        struct SState
        {
            TValue Value;
            uint64_t Revision = 0;
            std::array<uint64_t, GraphicsContext::FRAMES> AppliedRevisions{};
        };

        FrameSlotState<TResource> m_Resources;
        std::unordered_map<TKey, SState> m_States;
    };
}