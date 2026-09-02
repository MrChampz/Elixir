#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Outputs elapsed time in seconds.
     */
    class Time final : public MaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Time"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return { .Code = "Time", .ValueType = EMaterialValueType::Float };
        }
    };
}