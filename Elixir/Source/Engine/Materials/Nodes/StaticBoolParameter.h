#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Read a named compile-time boolean as a numeric zero or one. */
    class StaticBoolParameter final : public MaterialNode
    {
    public:
        /**
         * @brief Create a compile-time boolean reader.
         * @param name Static parameter overridden through MaterialInstance::SetStaticBool.
         * @param defaultValue Value used when the instance has no override.
         */
        explicit StaticBoolParameter(std::string name, const bool defaultValue = false)
          : m_Name(std::move(name)),
            m_Default(defaultValue) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override
        {
            return "Material.StaticBoolParameter";
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            bool value = m_Default;

            if (const auto bindings = context.GetBindings())
            {
                const auto found = bindings->StaticValues.find(m_Name);
                if (found != bindings->StaticValues.end())
                    value = found->second;
            }

            return {
                value ? "1.0" : "0.0",
                EMaterialValueType::Float
            };
        }

    private:
        std::string m_Name;
        bool m_Default;
    };
}
