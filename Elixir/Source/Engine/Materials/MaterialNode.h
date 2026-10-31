#pragma once

#include <Engine/Graphics/GraphicsTypes.h>

namespace Elixir::Materials
{
    namespace Compilation
    {
        class MaterialGraphEmitter;
        struct SMaterialGraphBindings;
        struct SMaterialEmissionEnvironment;
    }
    using namespace Compilation;

    namespace Nodes { struct STextureSampleSettings; }
    using namespace Nodes;

    class MaterialFunction;
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
     * @brief Gives a node access to resolved inputs and material parameter bindings.
     */
    class ELIXIR_API MaterialEmitContext
    {
        friend class MaterialGraphEmitter;

    public:
        /** @brief Gets a resolved input expression. */
        const SMaterialExpression& Input(uint32_t slot) const;

        /** @brief Gets the HLSL expression bound to a value parameter. */
        std::string ValueParameter(const std::string& name) const;

        /** @brief Gets the HLSL expression bound to a texture parameter. */
        std::string TextureParameter(const std::string& name) const;

        /**
         * @brief Reads an argument of the enclosing material function.
         * @param name Function input name.
         * @param type Numeric type used for conversion.
         */
        SMaterialExpression FunctionInput(
            const std::string& name,
            EMaterialValueType type
        ) const;

        /** @brief Converts an expression to a requested value type. */
        static std::string Widen(
            const SMaterialExpression& expression,
            EMaterialValueType type
        );

        /** @brief Gets the HLSL type name for a material value type. */
        static const char* TypeName(EMaterialValueType type);

        /** @brief Gets the component count of a material value type. */
        static int Components(EMaterialValueType type);

        /** @brief Gets the wider of two material value types. */
        static EMaterialValueType Wider(EMaterialValueType left, EMaterialValueType right);

        /** @brief Returns the bindings inherited by nested expressions. */
        const SMaterialGraphBindings* GetBindings() const { return m_Bindings; }

        /** @brief Return the world position available to this shader evaluation. */
        std::string WorldPosition() const;

        /**
         * @brief Return a texture-coordinate channel.
         * @param index Zero-based coordinate channel.
         */
        std::string TexCoord(uint32_t index = 0) const;

        /** @brief Return elapsed shader time in seconds. */
        std::string Time() const;

        /** @brief Return the color supplied by the geometry renderer. */
        std::string Color() const;

        /** @brief Return the normal available at the graph evaluation point. */
        std::string Normal() const;

        /** @brief Return the view direction available at the graph evaluation point. */
        std::string ViewDirection() const;

        /**
         * @brief Resolve an unconnected input from its semantic or literal default.
         * @param input Input definition.
         */
        SMaterialExpression DefaultInput(const SMaterialNodeInput& input) const;

        /**
         * @brief Sample a texture using the resources and capabilities of this stage.
         * @param name Texture parameter name.
         * @param uv Resolved texture coordinates.
         * @param mip Resolved LOD or bias.
         * @param settings Sampler and mip selection.
         */
        std::string SampleTexture(
            const std::string& name,
            const SMaterialExpression& uv,
            const SMaterialExpression& mip,
            const STextureSampleSettings& settings
        ) const;

        /**
         * @brief Evaluate a reusable function with this context's inputs and environment.
         * @param function Immutable function to evaluate.
         */
        SMaterialExpression CallFunction(const MaterialFunction& function) const;

        /** @brief Return the single shader stage used by this emission. */
        EShaderStage GetStage() const;

        /**
         * @brief Check that a stage identifies one concrete shader stage.
         * @param stage Stage to check; masks and sentinel values are rejected.
         */
        static bool IsValidStage(EShaderStage stage);

    private:
        /**
         * @brief Bind inputs and parameters for one shader emission.
         * @param inputs Resolved node arguments.
         * @param bindings Material parameters and function arguments.
         * @param environment Inputs and resources available in this template.
         * @param diagnostics Receives the first unavailable-input diagnostic.
         */
        MaterialEmitContext(
            const std::vector<SMaterialExpression>& inputs,
            const SMaterialGraphBindings* bindings,
            const SMaterialEmissionEnvironment& environment,
            std::string& diagnostics
        );

        /**
         * @brief Resolve an input or record a missing-resource diagnostic.
         * @param expression HLSL supplied by the template environment.
         * @param name Semantic input used in diagnostics.
         */
        std::string Require(const std::string& expression, std::string_view name) const;

        const std::vector<SMaterialExpression>& m_Inputs;
        const SMaterialGraphBindings* m_Bindings;
        const SMaterialEmissionEnvironment& m_Environment;
        std::string& m_Diagnostics;
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
