#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Select one numeric input at compile time without emitting the other branch. */
    class StaticSwitch final : public MaterialNode
    {
    public:
        /**
         * @brief Create a named compile-time choice.
         * @param name Static parameter name.
         * @param defaultValue Choice used when the compiler supplies no override.
         */
        explicit StaticSwitch(std::string name, const bool defaultValue = false)
          : MaterialNode({
              {
                    "True",
                    EMaterialValueType::Float4,
                    "0.0",
                    EMaterialValueType::Float
                },
                {
                    "False",
                    EMaterialValueType::Float4,
                    "0.0",
                    EMaterialValueType::Float
                }
            }),
            m_Name(std::move(name)),
            m_Default(defaultValue) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.StaticSwitch"; }

        /** @copydoc MaterialNode::SelectInput */
        std::optional<uint32_t> SelectInput(
            const SMaterialGraphBindings* bindings
        ) const override
        {
            if (bindings)
            {
                const auto found = bindings->StaticValues.find(m_Name);
                if (found != bindings->StaticValues.end())
                    return found->second ? 0 : 1;
            }

            return m_Default ? 0 : 1;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto input = SelectInput(context.GetBindings());
            return context.Input(*input);
        }

    private:
        std::string m_Name;
        bool m_Default;
    };
}
