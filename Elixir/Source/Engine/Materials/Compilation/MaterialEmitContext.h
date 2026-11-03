#pragma once

#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials
{
    class MaterialFunction;
}

namespace Elixir::Materials::Compilation
{
    struct SMaterialEmissionEnvironment;
    struct STextureSamplingOptions;

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
         * @param options Sampler and mip selection.
         */
        std::string SampleTexture(
            const std::string& name,
            const SMaterialExpression& uv,
            const SMaterialExpression& mip,
            const STextureSamplingOptions& options
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
         * @param emitter Receives requests to evaluate reusable functions.
         * @param diagnostics Receives the first unavailable-input diagnostic.
         */
        MaterialEmitContext(
            const std::vector<SMaterialExpression>& inputs,
            const SMaterialGraphBindings* bindings,
            const SMaterialEmissionEnvironment& environment,
            MaterialGraphEmitter& emitter,
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
        MaterialGraphEmitter& m_Emitter;
        std::string& m_Diagnostics;
    };
}
