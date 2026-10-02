#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Offsets texture coordinates over time.
     */
    class Panner final : public MaterialNode
    {
    public:
        explicit Panner(const glm::vec2& speed)
          : MaterialNode({{
                "UV",
                EMaterialValueType::Float2,
                "input.TexCoord",
                EMaterialValueType::Float2
            }}),
            m_Speed(speed) {}

        std::string_view GetTypeName() const override { return "Material.Panner"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto speed = "float2(" + std::to_string(m_Speed.x) + ", " +
                std::to_string(m_Speed.y) + ")";
            return {
                .Code = "(" + context.Widen(context.Input(0), EMaterialValueType::Float2) +
                    " + Time * " + speed + ")",
                .ValueType = EMaterialValueType::Float2
            };
        }

    private:
        glm::vec2 m_Speed;
    };
}