#pragma once

#include <algorithm>
#include <array>

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Selects one component from an input value. */
    class ComponentMaskNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a component mask that selects a zero-based component. */
        explicit ComponentMaskNode(const uint32_t componentIndex)
            : m_ComponentIndex(componentIndex),
              m_Inputs{{ "Value", EMaterialValueType::Float4, "0.0" }} {}

        std::string_view GetTypeName() const override { return "material.component_mask"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            static constexpr std::array components{ ".x", ".y", ".z", ".w" };
            const auto component = std::min(m_ComponentIndex, uint32_t(components.size() - 1));
            return { "(" + context.Input(0).Code + ")" + components[component], EMaterialValueType::Float };
        }

    private:
        uint32_t m_ComponentIndex;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
