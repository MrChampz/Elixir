#include "epch.h"
#include "MaterialFunction.h"
#include "Compilation/MaterialGraphEmitter.h"

namespace Elixir::Materials
{
    namespace
    {
        class FunctionLookup final : public MaterialNodeValidationContext
        {
        public:
            /**
             * @brief Bind the caller's schema and this function's arguments.
             * @param parent Caller schema.
             * @param inputs Arguments declared by this function.
             */
            FunctionLookup(
                const MaterialNodeValidationContext& parent,
                const std::vector<SMaterialFunctionInput>& inputs
            ) : m_Parent(parent),
                m_Inputs(inputs) {}

            /**
             * @brief Check a value parameter against the caller's schema.
             * @param name Value parameter name.
             * @param type Expected value type.
             */
            bool HasValueParameter(
                const std::string_view name,
                const EMaterialValueType type
            ) const override
            {
                return m_Parent.HasValueParameter(name, type);
            }

            /**
             * @brief Check whether the caller declares a texture parameter.
             * @param name Texture parameter name.
             */
            bool HasTextureParameter(const std::string_view name) const override
            {
                return m_Parent.HasTextureParameter(name);
            }

            /**
             * @brief Check an argument declared by this function.
             * @param name Function argument name.
             * @param type Expected argument type.
             */
            bool HasFunctionInput(
                const std::string_view name,
                const EMaterialValueType type
            ) const override
            {
                for (const auto& input : m_Inputs)
                {
                    if (input.Name == name)
                        return input.ValueType == type;
                }

                return false;
            }

            /** @brief Return the caller's renderer usage. */
            EMaterialUsage GetUsage() const override { return m_Parent.GetUsage(); }

        private:
            const MaterialNodeValidationContext& m_Parent;
            const std::vector<SMaterialFunctionInput>& m_Inputs;
        };
    }

    MaterialFunction::MaterialFunction(
        MaterialGraph graph,
        std::vector<SMaterialFunctionInput> inputs,
        const uint32_t output
    ) : m_Graph(std::move(graph)),
        m_Inputs(std::move(inputs)),
        m_Output(output) {}

    bool MaterialFunction::Validate(
        const MaterialNodeValidationContext& parameters,
        std::string& error
    ) const
    {
        if (!m_Graph.FindNode(m_Output))
        {
            error = "Material function output does not exist.";
            return false;
        }

        std::unordered_set<std::string> names;

        for (const auto& input : m_Inputs)
        {
            if (input.Name.empty() || !names.insert(input.Name).second)
            {
                error = "Material function inputs require unique, nonempty names.";
                return false;
            }
        }

        const MaterialGraphEmitter emitter(m_Graph, {});
        return emitter.Validate(FunctionLookup(parameters, m_Inputs), &error);
    }

    EShaderStage MaterialFunction::GetSupportedStages(
        const SMaterialGraphBindings* bindings
    ) const
    {
        const MaterialGraphEmitter emitter(m_Graph, {});
        return emitter.GetSupportedStages(m_Output, bindings);
    }
}
