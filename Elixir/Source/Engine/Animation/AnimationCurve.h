#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    /** Selects interpolation between adjacent scalar keys. */
    enum class EAnimationCurveInterpolation : uint8_t
    {
        Step, Linear
    };

    /** Stores a finite scalar value at a sequence time in seconds. */
    struct SAnimationCurveKey
    {
        float Time = 0.0f;
        float Value = 0.0f;
    };

    /** Defines a named scalar track independent of bone channels. */
    struct SAnimationCurve
    {
        std::string Name;

        /** Nonempty keys with strictly increasing times within the sequence duration. */
        std::vector<SAnimationCurveKey> Keys;

        EAnimationCurveInterpolation Interpolation = EAnimationCurveInterpolation::Linear;
    };

    /** Stores evaluated named scalars; missing names read as zero. */
    class ELIXIR_API AnimationCurveValues
    {
    public:
        /**
         * @brief Read a sampled value, or zero when the name is absent.
         * @param name Case-sensitive authored curve name.
         */
        float GetValue(std::string_view name) const;

        /** @brief Get sampled values ordered by name. */
        const std::map<std::string, float, std::less<>>& GetValues() const { return m_Values; }

        /**
         * @brief Store a finite named value; preserve values on invalid input.
         * @param name Nonempty case-sensitive curve name.
         * @param value Finite scalar, without a fixed range.
         */
        bool SetValue(std::string name, float value);

        /**
         * @brief Blend toward another sample, treating absent names on either side as zero.
         * @param target Sample to blend toward; may be this object.
         * @param weight Finite contribution in [0, 1].
         * @return False for invalid input; previous values remain unchanged.
         */
        bool Blend(const AnimationCurveValues& target, float weight);

        /**
         * @brief Apply only curves authored by a layer, preserving unrelated names.
         * @param layer Sampled layer values; may be this object.
         * @param weight Finite layer contribution in [0, 1], independent of bone weights.
         * @param additive Add weighted values to the result instead of overriding them.
         * @return False on invalid input or overflow; previous values remain unchanged.
         */
        bool ApplyLayer(const AnimationCurveValues& layer, float weight, bool additive);

    private:
        std::map<std::string, float, std::less<>> m_Values;
    };
}
