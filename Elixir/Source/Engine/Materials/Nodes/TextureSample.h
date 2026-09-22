#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Defines how a texture is interpreted by a material.
     *
     * The type lets the compiler select semantic decoding without exposing that
     * implementation detail in the material graph.
     */
    enum class ETextureSampleType : uint8_t
    {
        /** Texture stores display color. */
        Color,

        /** Texture stores linear color data. */
        LinearColor,

        /** Texture stores a tangent-space normal map. */
        Normal,

        /** Texture stores non-color mask data. */
        Mask,
    };

    /**
     * @brief Samples a named texture material parameter using a declared semantic.
     */
    class TextureSample final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a texture sampler for a material texture parameter.
         * @param parameterName Texture parameter name.
         * @param sampleType Semantic type used to interpret the sampled texture.
         */
        explicit TextureSample(
            std::string parameterName,
            const ETextureSampleType sampleType = ETextureSampleType::Color
        ) : MaterialNode({{
                "UV",
                EMaterialValueType::Float2,
                "input.TexCoord",
                EMaterialValueType::Float2
            }}),
            m_ParameterName(std::move(parameterName)),
            m_SampleType(sampleType) {}

        std::string_view GetTypeName() const override { return "Material.TextureSample"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto index = context.TextureParameter(m_ParameterName);
            const auto uv = context.Widen(
                context.Input(0),
                EMaterialValueType::Float2
            );

            if (m_SampleType == ETextureSampleType::Normal)
            {
                return {
                    .Code = "SampleNormal(" + index + ", " + uv + ")",
                    .ValueType = EMaterialValueType::Float3,
                };
            }

            return {
                .Code = "(" + index + " == 0xFFFFFFFFu " +
                    "? float4(1.0, 1.0, 1.0, 1.0) " +
                    ": SampleTex(" + index + ", " + uv + "))",
                .ValueType = EMaterialValueType::Float4
            };
        }

    private:
        std::string m_ParameterName;
        ETextureSampleType m_SampleType;
    };
}
