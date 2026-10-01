#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Outputs the RGBA color supplied by the particle renderer.
     */
    class Color final : public MaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Color"; }

        bool Validate(
            const MaterialNodeValidationContext& context,
            std::string& error
        ) const override
        {
            if (context.GetUsage() == EMaterialUsage::Particle)
                return true;

            error = "Material.Color requires particle material usage.";
            return false;
        }

        SMaterialExpression Emit(const MaterialEmitContext&) const override
        {
            return {
                .Code = "input.Color",
                .ValueType = EMaterialValueType::Float4
            };
        }
    };
}
