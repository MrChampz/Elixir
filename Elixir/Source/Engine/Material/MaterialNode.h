#pragma once

#include <vector>
#include <string_view>

namespace Elixir
{
    struct SMaterialGraphBindings;
    enum class EMaterialValueType : uint8_t;

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
        virtual bool HasValueParameter(std::string_view name, EMaterialValueType type) const = 0;

        /**
         * @brief Checks whether a texture parameter exists.
         * @param name The parameter name.
         * @return True if the texture parameter exists.
         */
        virtual bool HasTextureParameter(std::string_view name) const = 0;
    };

    /**
     * @brief Gives a node access to resolved inputs and material parameter bindings.
     */
    class MaterialEmitContext
    {
        friend class MaterialGraph;

    public:
        /** @brief Gets a resolved input expression. */
        const SMaterialExpression& Input(uint32_t slot) const;

        /** @brief Gets the HLSL expression bound to a value parameter. */
        std::string ValueParameter(const std::string& name) const;

        /** @brief Gets the HLSL expression bound to a texture parameter. */
        std::string TextureParameter(const std::string& name) const;

        /** @brief Converts an expression to a requested value type. */
        static std::string Widen(const SMaterialExpression& expression, EMaterialValueType type);

        /** @brief Gets the HLSL type name for a material value type. */
        static const char* TypeName(EMaterialValueType type);

        /** @brief Gets the component count of a material value type. */
        static int Components(EMaterialValueType type);

        /** @brief Gets the wider of two material value types. */
        static EMaterialValueType Wider(EMaterialValueType left, EMaterialValueType right);

    private:
        MaterialEmitContext(
            const std::vector<SMaterialExpression>& inputs,
            const SMaterialGraphBindings* bindings
        );

        const std::vector<SMaterialExpression>& m_Inputs;
        const SMaterialGraphBindings* m_Bindings;
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

        /** @brief Validates node-specific references against material parameters. */
        virtual bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const
        {
            return true;
        }

        /** @brief Emits the HLSL expression represented by this node. */
        virtual SMaterialExpression Emit(const MaterialEmitContext& context) const = 0;

        /** @brief Gets the input slots defined by the node. */
        const std::vector<SMaterialNodeInput>& GetInputs() const { return m_Inputs; }

    protected:
        explicit MaterialNode(std::vector<SMaterialNodeInput> inputs = {})
          : m_Inputs(std::move(inputs)) {}

        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
