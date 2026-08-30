#pragma once

#include <Engine/Material/MaterialGraph.h>
#include <glm/glm.hpp>

namespace Elixir::MaterialNodes
{
    /** @brief Offsets texture coordinates over time. */
    class PannerNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a panner with UV speed in units per second. */
        explicit PannerNode(const glm::vec2& speed)
            : m_Speed(speed), m_Inputs{{ "UV", EMaterialValueType::Float2, "input.TexCoord", EMaterialValueType::Float2 }} {}

        std::string_view GetTypeName() const override { return "material.panner"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            const auto speed = "float2(" + std::to_string(m_Speed.x) + ", " + std::to_string(m_Speed.y) + ")";
            return { "(" + context.Widen(context.Input(0), EMaterialValueType::Float2) + " + Time * " + speed + ")", EMaterialValueType::Float2 };
        }

    private:
        glm::vec2 m_Speed;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
