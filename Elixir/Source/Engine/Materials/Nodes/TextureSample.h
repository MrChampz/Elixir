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

    /** Select texture addressing outside the unit UV rectangle. */
    enum class ETextureSampleAddress : uint8_t
    {
        Wrap, Clamp
    };

    /** Select interpolation between texels. */
    enum class ETextureSampleFilter : uint8_t
    {
        Linear, Point
    };

    /** Select implicit derivatives, an explicit level, or a derivative bias. */
    enum class ETextureSampleMipMode : uint8_t
    {
        Auto, Level, Bias
    };

    /** Select the numeric components returned by a sample. */
    enum class ETextureSampleOutput : uint8_t
    {
        Default,
        R,
        G,
        B,
        A,
        RGB,
        RGBA
    };

    /** Configure texture sampling independently of the parameter value. */
    struct STextureSampleSettings
    {
        ETextureSampleAddress Address  = ETextureSampleAddress::Wrap;
        ETextureSampleFilter  Filter   = ETextureSampleFilter::Linear;
        ETextureSampleMipMode MipMode  = ETextureSampleMipMode::Auto;
        ETextureSampleOutput  Output   = ETextureSampleOutput::Default;
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
                "input.TexCoord",
                EMaterialValueType::Float2
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
            return m_Settings.MipMode == ETextureSampleMipMode::Bias
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
            const auto index = context.TextureParameter(m_ParameterName);
            const auto uv = context.Widen(context.Input(0), EMaterialValueType::Float2);
            const auto mip = context.Widen(context.Input(1), EMaterialValueType::Float);
            const auto bindings = context.GetBindings();

            const auto array = bindings
                ? bindings->TextureArray
                : "textures";

            std::string sampler = bindings
                ? bindings->TextureSampler
                : "texSampler";

            const bool clamp = m_Settings.Address == ETextureSampleAddress::Clamp;

            if (m_Settings.Filter == ETextureSampleFilter::Point)
                sampler = clamp ? "pointClamp" : "pointWrap";
            else if (clamp)
                sampler = "linearClamp";

            const bool explicitLod = context.GetStage() != EShaderStage::Pixel;

            std::string sample;
            if (explicitLod || m_Settings.MipMode == ETextureSampleMipMode::Level)
            {
                const auto level = m_Settings.MipMode == ETextureSampleMipMode::Auto
                    ? std::string("0.0")
                    : mip;

                sample = array + "[" + index + "]"
                    ".SampleLevel(" + sampler + ", " + uv + ", " + level + ")";
            }
            else if (m_Settings.MipMode == ETextureSampleMipMode::Bias)
            {
                sample = array + "[" + index + "]"
                    ".SampleBias(" + sampler + ", " + uv + ", " + mip + ")";
            }
            else
            {
                sample = array + "[" + index + "].Sample(" + sampler + ", " + uv + ")";
            }

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
