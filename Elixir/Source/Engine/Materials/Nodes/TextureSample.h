#pragma once

#include <Engine/Materials/Compilation/TextureSamplingOptions.h>
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

    /** Select the numeric components returned by a sample. */
    enum class ETextureSampleOutput : uint8_t
    {
        Default, R, G, B, A, RGB, RGBA
    };

    /** Configure texture sampling independently of the parameter value. */
    struct STextureSampleSettings
    {
        ETextureAddress Address = ETextureAddress::Wrap;
        ETextureFilter  Filter  = ETextureFilter::Linear;
        ETextureMipMode MipMode = ETextureMipMode::Auto;
        ETextureSampleOutput Output  = ETextureSampleOutput::Default;
    };

    /** @brief Samples a named texture material parameter using a declared semantic. */
    class TextureSample final : public MaterialNode
    {
    public:
        /**
        * @brief Creates a texture sampler for a material texture parameter.
        * @param parameterName Texture parameter name.
        * @param type Semantic type used to interpret the sampled texture.
        * @param settings Addressing, filtering, mip selection, and output components.
        */
        explicit TextureSample(
          std::string parameterName,
          const ETextureSampleType type = ETextureSampleType::Color,
          const STextureSampleSettings settings = {}
        ) : MaterialNode({
                {
                    "UV",
                    EMaterialValueType::Float2,
                    "",
                    EMaterialValueType::Float2,
                    EMaterialInputDefaultSource::TexCoord
                },
                {
                    "Mip",
                    EMaterialValueType::Float,
                    "0.0",
                    EMaterialValueType::Float
                }
            }),
            m_ParameterName(std::move(parameterName)),
            m_SampleType(type),
            m_Settings(settings) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.TextureSample"; }

        /** @copydoc MaterialNode::GetSupportedStages */
        EShaderStage GetSupportedStages(const SMaterialGraphBindings* bindings) const override
        {
            return m_Settings.MipMode == ETextureMipMode::Bias
                ? EShaderStage::Pixel
                : EShaderStage::All;
        }

        /** @copydoc MaterialNode::Validate */
        bool Validate(
          const MaterialNodeValidationContext& parameters,
          std::string& error
        ) const override
        {
            if (parameters.HasTextureParameter(m_ParameterName))
                return true;

            error = "Unknown texture parameter: " + m_ParameterName;
            return false;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const STextureSamplingOptions options{
                .Address = m_Settings.Address,
                .Filter = m_Settings.Filter,
                .MipMode = m_Settings.MipMode
            };

            const auto sample = context.SampleTexture(
                m_ParameterName,
                context.Input(0),
                context.Input(1),
                options
            );

            const auto index = context.TextureParameter(m_ParameterName);
            const bool normal = m_SampleType == ETextureSampleType::Normal;

            const auto fallback = normal
                ? "float4(0.5, 0.5, 1.0, 1.0)"
                : "float4(1.0, 1.0, 1.0, 1.0)";

            auto raw = "(" + index + " == 0xFFFFFFFFu ? " + fallback + " : " + sample + ")";

            if (normal)
                raw = "DecodeNormal(" + raw + ")";

            auto output = m_Settings.Output;
            if (output == ETextureSampleOutput::Default)
                output = normal ? ETextureSampleOutput::RGB : ETextureSampleOutput::RGBA;

            if (output == ETextureSampleOutput::RGBA)
                return { raw, EMaterialValueType::Float4 };

            if (output == ETextureSampleOutput::RGB)
                return { raw + ".rgb", EMaterialValueType::Float3 };

            auto component = "r";

            if (output == ETextureSampleOutput::G)
                component = "g";

            if (output == ETextureSampleOutput::B)
                component = "b";

            if (output == ETextureSampleOutput::A)
                component = "a";

            return { raw + "." + component, EMaterialValueType::Float };
        }

    private:
        std::string m_ParameterName;
        ETextureSampleType m_SampleType;
        STextureSampleSettings m_Settings;
    };
}
