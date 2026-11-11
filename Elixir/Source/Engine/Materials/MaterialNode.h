#pragma once

#include <Engine/Graphics/GraphicsTypes.h>

namespace Elixir::Materials
{
    namespace Compilation
    {
        class MaterialGraphEmitter;
        class MaterialEmitContext;
        struct SMaterialGraphBindings;
    }
    using namespace Compilation;

    enum class EMaterialValueType : uint8_t;
    enum class EMaterialUsage : uint8_t;

    /**
     * @brief Select the default value source for an unconnected material node input.
     * Connected inputs use their source node and ignore this setting.
    */
    enum class EMaterialInputDefaultSource : uint8_t
    {
        /** Use DefaultExpression with the type declared by DefaultValueType. */
        Expression,

        /** Resolve texture-coordinate channel zero through the emission context. */
        TexCoord,

        /** Resolve texture-coordinate channel one through the emission context. */
        TexCoord1
    };

    /**
     * @brief Describes one input accepted by a material node.
     */
    struct SMaterialNodeInput
    {
        /** @brief User-facing input name. */
        std::string_view Name;

        /** @brief Type expected by the node. */
        EMaterialValueType ValueType;

        /** @brief HLSL expression used when the input is not connected. */
        std::string DefaultExpression;

        /** @brief Type produced by the default expression. */
        EMaterialValueType DefaultValueType;

        /** Default value source used when this input is not connected.  */
        EMaterialInputDefaultSource DefaultSource = EMaterialInputDefaultSource::Expression;
    };

    /**
     * @brief Stores an HLSL expression and its material value type.
     */
    struct SMaterialExpression
    {
        std::string Code;
        EMaterialValueType ValueType;
    };

    /**
     * @brief Lets a node verify references to the material parameter schema.
     */
    class MaterialNodeValidationContext
    {
    public:
        /** @brief Destroys the validation context. */
        virtual ~MaterialNodeValidationContext() = default;

        /**
         * @brief Checks whether a value parameter has the requested type.
         * @param name The parameter name.
         * @param type The parameter type.
         * @return True if the value parameter exists for the requested type.
         */
        virtual bool HasValueParameter(
            std::string_view name,
            EMaterialValueType type
        ) const = 0;

        /**
         * @brief Checks whether a texture parameter exists.
         * @param name The parameter name.
         * @return True if the texture parameter exists.
         */
        virtual bool HasTextureParameter(std::string_view name) const = 0;

        /**
         * @brief Checks whether a texture atlas resource parameter exists.
         * @param name Parameter name.
         * @return Whether the parameter accepts texture atlas resources.
         */
        virtual bool HasTextureAtlasParameter(std::string_view name) const { return false; }
        
        /**
         * @brief Checks an input declared by the enclosing material function.
         * @param name Function input name.
         * @param type Expected numeric type.
         */
        virtual bool HasFunctionInput(std::string_view name, EMaterialValueType type) const
        {
            return false;
        }

        /** @brief Returns the renderer usage of the material being validated. */
        virtual EMaterialUsage GetUsage() const = 0;
    };

    /**
     * @brief Defines one operation that can be placed in a material graph.
     */
    class MaterialNode
    {
    public:
        virtual ~MaterialNode() = default;

        /** @brief Gets the stable identifier used by tools and serialization. */
        virtual std::string_view GetTypeName() const = 0;

        /**
         * @brief Validates node-specific references against material parameters.
         * @param parameters Parameter and function-argument schema.
         * @param error Receives a diagnostic on failure.
         */
        virtual bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const
        {
            return true;
        }

        /**
         * @brief Emits the HLSL expression represented by this node.
         * @param context Resolved inputs and compile-time bindings.
         */
        virtual SMaterialExpression Emit(const MaterialEmitContext& context) const = 0;

        /**
         * @brief Selects one input at compile time, or all inputs when empty.
         * @param bindings Compile-time graph bindings.
         */
        virtual std::optional<uint32_t> SelectInput(
            const SMaterialGraphBindings* bindings
        ) const
        {
            return std::nullopt;
        }

        /**
         * @brief Return the shader stages supported by this node.
         * @param bindings Compile-time choices inherited from the caller.
         */
        virtual EShaderStage GetSupportedStages(
            const SMaterialGraphBindings* bindings = nullptr
        ) const
        {
            return EShaderStage::All;
        }

        /** @brief Gets the input slots defined by the node. */
        const std::vector<SMaterialNodeInput>& GetInputs() const { return m_Inputs; }

    protected:
        explicit MaterialNode(std::vector<SMaterialNodeInput> inputs = {})
          : m_Inputs(std::move(inputs)) {}

        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
