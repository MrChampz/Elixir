#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Generates a procedural checkerboard.
     */
    class Checkerboard final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a checkerboard with the requested number of cells.
         * @param scale The checkerboard scale.
         */
        explicit Checkerboard(const float scale)
          : m_Scale(scale),
            m_Inputs{{
                "UV",
                EMaterialValueType::Float2,
                "input.TexCoord",
                EMaterialValueType::Float2
            }} {}

        std::string_view GetTypeName() const override { return "Material.Checkerboard"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto uv = context.Widen(context.Input(0), EMaterialValueType::Float2);
            const auto scale = std::to_string(std::max(m_Scale, 1.0f));
            return {
                "(fmod(floor(" + uv + ".x * " + scale + ") + floor(" + uv + ".y * " +
                    scale + "), 2.0) < 1.0" +
                        "? float3(0.08, 0.08, 0.08)" +
                        ": float3(0.72, 0.72, 0.72))",
                EMaterialValueType::Float3
            };
        }

    private:
        float m_Scale;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}