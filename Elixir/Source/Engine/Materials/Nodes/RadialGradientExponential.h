#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Generates an exponential radial gradient.
     */
    class RadialGradientExponential final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a radial gradient that fades from its center.
         * @param center Center in UV coordinates.
         * @param radius Distance from the center where the gradient reaches zero.
         * @param exponent Controls the falloff shape. Higher values create a sharper fade.
         */
        RadialGradientExponential(
            const glm::vec2& center,
            const float radius,
            const float exponent
        ) : MaterialNode({{
                "UV",
                EMaterialValueType::Float2,
                "input.TexCoord",
                EMaterialValueType::Float2
            }}),
            m_Center(center),
            m_Radius(radius),
            m_Exponent(exponent) {}

        std::string_view GetTypeName() const override { return "Material.RadialGradientExponential"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto uv = context.Widen(context.Input(0), EMaterialValueType::Float2);
            const auto center = "float2(" + std::to_string(m_Center.x) + ", " + std::to_string(m_Center.y) + ")";
            const auto radius = std::to_string(std::max(m_Radius, 0.0001f));
            const auto exponent = std::to_string(std::max(m_Exponent, 0.0001f));
            return {
                .Code = "pow(saturate(1.0 - length((" + uv + " - " + center + ") / " + radius +
                    ")), " + exponent + ")",
                .ValueType = EMaterialValueType::Float
            };
        }

    private:
        glm::vec2 m_Center;
        float m_Radius;
        float m_Exponent;
    };
}