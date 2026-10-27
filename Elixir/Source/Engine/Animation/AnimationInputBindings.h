#pragma once

#include <Engine/Animation/AnimationParameters.h>

namespace Elixir
{
    /** Owns numeric inputs and their optional connection to character parameters. */
    class ELIXIR_API AnimationInputBindings
    {
    public:
        /**
         * @brief Retain parameters and resolve their current numeric values.
         * @param parameters Shared character parameters.
         * @param bindings Indices of numeric parameters, in input order.
         */
        bool Bind(Ref<AnimationParameters> parameters, std::span<const size_t> bindings);

        /**
         * @brief Replace the owned numeric values without removing their bindings.
         * @param values Finite coordinates in input order.
         */
        bool SetValues(std::span<const float> values);

        /** @brief Refresh owned values from bound character parameters. */
        bool Refresh();

        /** @brief Get the owned coordinates. */
        std::span<const float> GetValues() const { return m_Values; }

        /** @brief Get the shared character parameters, if bound. */
        const Ref<AnimationParameters>& GetParameters() const { return m_Parameters; }

    private:
        Ref<AnimationParameters> m_Parameters;
        std::vector<size_t> m_Indices;
        std::vector<float> m_Values;
    };
}
