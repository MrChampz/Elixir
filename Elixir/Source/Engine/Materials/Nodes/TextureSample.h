#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Samples a named texture material parameter.
     */
    class TextureSample final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a texture sampler for a material texture parameter.
         * @param parameterName Texture parameter name.
         */
        explicit TextureSample(std::string parameterName)
          : MaterialNode({{
                "UV",
                EMaterialValueType::Float2,
                "input.TexCoord",
                EMaterialValueType::Float2
            }}),
            m_ParameterName(std::move(parameterName)) {}

        std::string_view GetTypeName() const override { return "Material.TextureSample"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto index = context.TextureParameter(m_ParameterName);
            return {
                .Code = "(" + index + " == 0xFFFFFFFFu " +
                    "? float4(1.0, 1.0, 1.0, 1.0) " +
                    ": SampleTex(" +
                        index + ", " +
                        context.Widen(context.Input(0), EMaterialValueType::Float2) +
                    "))",
                .ValueType = EMaterialValueType::Float4
            };
        }

    private:
        std::string m_ParameterName;
    };
}