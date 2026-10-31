#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Read the undisplaced world-space position of a vertex or fragment. */
    class Position final : public MaterialNode
    {
    public:
        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.Position"; }

        /** @copydoc MaterialNode::Validate */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const override
        {
            if (parameters.GetUsage() == EMaterialUsage::Surface)
                return true;

            error = "World position currently requires surface usage.";
            return false;
        }

        /** @copydoc MaterialNode::GetSupportedStages */
        EShaderStage GetSupportedStages(
            const SMaterialGraphBindings* bindings = nullptr
        ) const override
        {
            return EShaderStage::Vertex | EShaderStage::Pixel;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return { context.WorldPosition(), EMaterialValueType::Float3 };
        }
    };
}
