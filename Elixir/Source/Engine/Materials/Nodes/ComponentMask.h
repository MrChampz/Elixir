#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Selects one component from an input value.
     */
    class ComponentMask final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a component mask that selects a zero-based component.
         * @param componentIndex Zero-based component index.
         */
        explicit ComponentMask(const uint32_t componentIndex)
          : MaterialNode({{
                .Name = "Value",
                .ValueType = EMaterialValueType::Float4,
                .DefaultExpression = "float4(0.0, 0.0, 0.0, 0.0)",
                .DefaultValueType = EMaterialValueType::Float4
            }}),
            m_ComponentIndex(componentIndex) {}

        std::string_view GetTypeName() const override { return "Material.ComponentMask"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            static constexpr std::array components{ ".x", ".y", ".z", ".w" };
            const auto component = std::min(m_ComponentIndex, uint32_t(components.size() - 1));
            return {
                .Code = "(" + context.Input(0).Code + ")" + components[component],
                .ValueType = EMaterialValueType::Float
            };
        }

    private:
        uint32_t m_ComponentIndex;
    };
}