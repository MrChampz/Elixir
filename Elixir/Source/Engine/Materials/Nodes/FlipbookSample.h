#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/TextureAtlas.h>
#include <Engine/Materials/Compilation/MaterialEmitContext.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Samples an atlas using a frame input supplied by any material graph. */
    class FlipbookSample final : public MaterialNode
    {
      public:
        /**
         * @brief Creates an atlas sampler with UV and zero-based frame inputs.
         * @param parameter Texture material parameter.
         * @param atlas Atlas layout and interpolation policy.
         */
        FlipbookSample(std::string parameter, STextureAtlas atlas)
            : MaterialNode(
                  {{"UV",
                    EMaterialValueType::Float2,
                    "",
                    EMaterialValueType::Float2,
                    EMaterialInputDefaultSource::TexCoord},
                   {"Frame", EMaterialValueType::Float, "0.0", EMaterialValueType::Float}}
              ),
              m_Parameter(std::move(parameter)), m_Atlas(atlas)
        {
        }
        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.FlipbookSample"; }
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
            if (!m_Atlas.IsValid())
            {
                error = "Invalid texture atlas layout.";
                return false;
            }
            if (!parameters.HasTextureParameter(m_Parameter))
            {
                error = "Unknown atlas texture parameter: " + m_Parameter;
                return false;
            }
            return true;
        }
        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {
                context.SampleTextureAtlas(
                    m_Parameter,
                    context.Input(0),
                    context.Input(1),
                    m_Atlas
                ),
                EMaterialValueType::Float4
            };
        }

      private:
        std::string m_Parameter;
        STextureAtlas m_Atlas;
    };
}
