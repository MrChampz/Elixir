#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/Compilation/MaterialEmitContext.h>

namespace Elixir::Materials::Nodes
{
    /** Selects how the frame input is interpreted when sampling an atlas resource. */
    enum class ETextureAtlasFrameMode : uint8_t
    {
        /** Zero-based frame index. Fractional values can interpolate adjacent frames. */
        Index,

        /**
         * Value from zero to one spanning the grid. Discrete playback gives each cell
         * an equal interval; interpolated playback blends from the first frame to the last.
         */
        Normalized
    };

    /** @brief Samples a texture atlas resource using UV and frame inputs. */
    class TextureAtlasSample final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a sampler whose grid comes from the bound atlas resource.
         * @param parameter Texture atlas resource parameter name.
         * @param interpolate Whether fractional frame indices blend adjacent frames.
         * @param frameMode Interpretation of the frame input.
         */
        explicit TextureAtlasSample(
            std::string parameter,
            const bool interpolate = false,
            const ETextureAtlasFrameMode frameMode = ETextureAtlasFrameMode::Index
        ) : MaterialNode({
                {
                    "UV",
                    EMaterialValueType::Float2,
                    "",
                    EMaterialValueType::Float2,
                    EMaterialInputDefaultSource::TexCoord
                },
                {
                    "Frame",
                    EMaterialValueType::Float,
                    "0.0",
                    EMaterialValueType::Float
                }
            }),
            m_Parameter(std::move(parameter)),
            m_Interpolate(interpolate),
            m_FrameMode(frameMode) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.TextureAtlasSample"; }

        /** @copydoc MaterialNode::GetSupportedStages */
        EShaderStage GetSupportedStages(const SMaterialGraphBindings* bindings) const override
        {
            return EShaderStage::Pixel;
        }

        /** @copydoc MaterialNode::Validate */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const override
        {
            if (parameters.HasTextureAtlasParameter(m_Parameter))
                return true;

            error = "Unknown texture atlas resource parameter: " + m_Parameter;
            return false;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto sample = context.SampleTextureAtlas(
                m_Parameter,
                context.Input(0),
                context.Input(1),
                m_Interpolate,
                m_FrameMode == ETextureAtlasFrameMode::Normalized
            );

            const auto index = context.TextureParameter(m_Parameter);
            constexpr auto fallback = "float4(1,1,1,1)";
            const auto raw = "(" + index + " == 0xFFFFFFFFu ? " + fallback + " : " + sample + ")";

            return { raw, EMaterialValueType::Float4 };
        }

    private:
        std::string m_Parameter;
        bool m_Interpolate;
        ETextureAtlasFrameMode m_FrameMode;
    };
}
