#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Outputs the input texture coordinates.
     */
    class TexCoord final : public MaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.TexCoord"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {
                .Code = "input.TexCoord",
                .ValueType = EMaterialValueType::Float2
            };
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}