#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Samples a named texture material parameter. */
    class TextureSampleNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a texture sampler for a material texture parameter. */
        explicit TextureSampleNode(std::string parameterName)
            : m_ParameterName(std::move(parameterName)),
              m_Inputs{{ "UV", EMaterialValueType::Float2, "input.TexCoord", EMaterialValueType::Float2 }} {}

        std::string_view GetTypeName() const override { return "material.texture_sample"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        bool Validate(const IMaterialNodeValidationContext& parameters, std::string& error) const override
        {
            if (parameters.HasTextureParameter(m_ParameterName)) return true;
            error = "Invalid texture parameter: " + m_ParameterName;
            return false;
        }

        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            const auto index = context.TextureParameter(m_ParameterName);
            return { "(" + index + " == 0xFFFFFFFFu ? float4(1.0, 1.0, 1.0, 1.0) : SampleTex(" +
                index + ", " + context.Widen(context.Input(0), EMaterialValueType::Float2) + "))",
                EMaterialValueType::Float4 };
        }

    private:
        std::string m_ParameterName;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
