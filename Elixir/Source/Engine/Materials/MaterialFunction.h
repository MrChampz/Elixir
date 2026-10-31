#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials
{
    /** Defines one typed argument and its default value. */
    struct SMaterialFunctionInput
    {
        std::string Name;
        EMaterialValueType ValueType = EMaterialValueType::Float;
        glm::vec4 DefaultValue{ 0.0f };
    };

    /**
     * @brief Own an immutable graph with typed inputs and one reusable output.
     * Construct dependencies before their callers. Functions may call other functions.
     */
    class ELIXIR_API MaterialFunction final
    {
      public:
        /**
         * @brief Take ownership of a reusable function graph.
         * @param graph Graph whose FunctionInput nodes read the declared arguments.
         * @param inputs Named arguments with numeric types and defaults.
         * @param output Node that provides the function result.
         */
        MaterialFunction(
            MaterialGraph graph,
            std::vector<SMaterialFunctionInput> inputs,
            uint32_t output
        );

        /**
         * @brief Validate arguments, output, and nested parameter references.
         * @param parameters Parameter schema inherited from the calling material.
         * @param error Receives a diagnostic on failure.
         */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const;

        /**
         * @brief Expand the output with the caller's parameter and argument bindings.
         * @param bindings Bindings for this call.
         * @param stage Concrete shader stage inherited from the caller.
         */
        SMaterialExpression Emit(
            const SMaterialGraphBindings& bindings,
            EShaderStage stage = EShaderStage::Pixel
        ) const;

        /**
         * @brief Return the shader stages supported by the function output.
         * @param bindings Compile-time choices inherited from the caller.
         */
        EShaderStage GetSupportedStages(
            const SMaterialGraphBindings* bindings = nullptr
        ) const;

        /** @brief Return the immutable argument declarations. */
        const auto& GetInputs() const { return m_Inputs; }

      private:
        const MaterialGraph m_Graph;
        const std::vector<SMaterialFunctionInput> m_Inputs;
        const uint32_t m_Output;
    };
}
