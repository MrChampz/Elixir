#pragma once

#include <string_view>
#include <vector>

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialGraph.h>

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
          : m_ParameterName(std::move(parameterName)),
            m_Inputs{{
                "UV",
                EMaterialValueType::Float2,
                "input.TexCoord",
                EMaterialValueType::Float2
            }} {}

        std::string_view GetTypeName() const override { return "Material.TextureSample"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

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
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}