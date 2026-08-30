#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Elixir
{
    /** @brief Defines the HLSL value type carried by a material graph connection. */
    enum class EMaterialValueType : uint8_t
    {
        /** @brief One floating-point component. */
        Float,
        /** @brief Two floating-point components. */
        Float2,
        /** @brief Three floating-point components. */
        Float3,
        /** @brief Four floating-point components. */
        Float4,
    };

    struct SMaterialGraphBindings;

    /** @brief Describes one input accepted by a material node. */
    struct SMaterialNodeInput
    {
        /** @brief User-facing input name. */
        std::string_view Name;
        /** @brief Type expected by the node. */
        EMaterialValueType ValueType;
        /** @brief HLSL expression used when the input is not connected. */
        std::string DefaultExpression;

        /** @brief Type produced by the default expression. */
        EMaterialValueType DefaultValueType = EMaterialValueType::Float;
    };

    /** @brief Stores an HLSL expression and its material value type. */
    struct SMaterialExpression
    {
        /** @brief HLSL expression text. */
        std::string Code;
        /** @brief Type produced by the expression. */
        EMaterialValueType ValueType;
    };

    /** @brief Lets a node verify references to the material parameter schema. */
    class IMaterialNodeValidationContext
    {
    public:
        virtual ~IMaterialNodeValidationContext() = default;
        /** @brief Checks whether a value parameter has the requested type. */
        virtual bool HasValueParameter(std::string_view name, EMaterialValueType valueType) const = 0;
        /** @brief Checks whether a texture parameter exists. */
        virtual bool HasTextureParameter(std::string_view name) const = 0;
    };

    /** @brief Gives a node access to resolved inputs and material parameter bindings. */
    class SMaterialEmitContext
    {
    public:
        /** @brief Gets a resolved input expression. */
        const SMaterialExpression& Input(uint32_t slot) const;
        /** @brief Gets the HLSL expression bound to a value parameter. */
        std::string ValueParameter(std::string_view name) const;
        /** @brief Gets the HLSL expression bound to a texture parameter. */
        std::string TextureParameter(std::string_view name) const;
        /** @brief Converts an expression to a requested value type. */
        std::string Widen(const SMaterialExpression& expression, EMaterialValueType type) const;
        /** @brief Gets the HLSL type name for a material value type. */
        static const char* TypeName(EMaterialValueType type);
        /** @brief Gets the component count of a material value type. */
        static int Components(EMaterialValueType type);
        /** @brief Gets the wider of two material value types. */
        static EMaterialValueType Wider(EMaterialValueType left, EMaterialValueType right);

    private:
        friend class MaterialGraph;
        SMaterialEmitContext(const std::vector<SMaterialExpression>& inputs, const SMaterialGraphBindings* bindings);
        const std::vector<SMaterialExpression>& m_Inputs;
        const SMaterialGraphBindings* m_Bindings;
    };

    /** @brief Defines one operation that can be placed in a material graph. */
    class IMaterialNode
    {
    public:
        virtual ~IMaterialNode() = default;
        /** @brief Gets the stable identifier used by tools and serialization. */
        virtual std::string_view GetTypeName() const = 0;
        /** @brief Gets the input slots defined by the node. */
        virtual const std::vector<SMaterialNodeInput>& GetInputs() const = 0;
        /** @brief Validates node-specific references against material parameters. */
        virtual bool Validate(const IMaterialNodeValidationContext& parameters, std::string& error) const;
        /** @brief Emits the HLSL expression represented by this node. */
        virtual SMaterialExpression Emit(const SMaterialEmitContext& context) const = 0;
    };
}
