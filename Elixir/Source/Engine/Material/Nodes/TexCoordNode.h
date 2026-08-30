#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Outputs the input texture coordinates. */
    class TexCoordNode final : public IMaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "material.tex_coord"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext&) const override
        {
            return { "input.TexCoord", EMaterialValueType::Float2 };
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
