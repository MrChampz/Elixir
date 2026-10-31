#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Evaluate smooth deterministic value noise from a two-dimensional position. */
    class Noise final : public MaterialNode
    {
    public:
        /** @brief Create a UV noise node with unit frequency. */
        Noise()
          : MaterialNode({
              {
                    "UV",
                    EMaterialValueType::Float2,
                    "",
                    EMaterialValueType::Float2,
                    EMaterialInputDefaultSource::TexCoord
              },
              {
                    "Scale",
                    EMaterialValueType::Float,
                    "1.0",
                    EMaterialValueType::Float
              }
            }) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.Noise"; }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto uv = context.Widen(context.Input(0), EMaterialValueType::Float2);
            const auto scale = context.Widen(context.Input(1), EMaterialValueType::Float);
            const auto point = "(" + uv + " * " + scale + ")";
            const auto cell = "floor(" + point + ")";
            const auto fraction = "frac(" + point + ")";
            const auto smooth =
                "(" + fraction + " * " + fraction + " * (3.0 - 2.0 * " + fraction + "))";

            const auto hash = [&](const char* offset)
            {
                return "frac(sin(dot(" + cell + " + " + offset +
                       ", float2(127.1, 311.7))) * 43758.5453)";
            };

            return {
                "lerp("
                    "lerp(" + hash("float2(0, 0)") + ", " + hash("float2(1, 0)") + ", " + smooth + ".x),"
                    "lerp(" + hash("float2(0, 1)") + ", " + hash("float2(1, 1)") + ", " + smooth + ".x)," +
                    smooth + ".y"
                ")",
                EMaterialValueType::Float
            };
        }
    };
}
