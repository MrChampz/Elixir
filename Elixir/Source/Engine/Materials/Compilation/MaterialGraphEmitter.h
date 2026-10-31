#pragma once

#include <Engine/Materials/MaterialGraph.h>
#include <Engine/Materials/Compilation/MaterialEmissionEnvironment.h>
#include <Engine/Materials/Compilation/MaterialGraphBindings.h>

namespace Elixir::Materials::Compilation
{
    /** @brief Traverse a material graph for validation and shader code emission. */
    class ELIXIR_API MaterialGraphEmitter final
    {
      public:
        /**
         * @brief Bind an immutable graph to one shader environment.
         * @param graph Graph kept alive throughout emission.
         * @param environment Available shader inputs and resources.
         * @param bindings Parameter expressions and compile-time arguments.
         */
        MaterialGraphEmitter(
            const MaterialGraph& graph,
            const SMaterialEmissionEnvironment& environment,
            const SMaterialGraphBindings& bindings = {}
        );

        /**
         * @brief Check graph topology, parameter references, and output stage support.
         * @param parameters Parameter schema of the owning material.
         * @param error Receives the first diagnostic.
         * @param bindings Static choices used to select active dependencies.
         */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string* error = nullptr,
            const SMaterialGraphBindings* bindings = nullptr
        ) const;

        /** @brief Generate statements for the current shader stage. */
        std::string GenerateHLSL();

        /**
         * @brief Expand one reusable function output as an inline expression.
         * @param nodeId Output node to evaluate.
         */
        SMaterialExpression GenerateExpression(uint32_t nodeId);

        /**
         * @brief Intersect the stage masks of an output and its active dependencies.
         * @param nodeId Output to inspect.
         * @param bindings Static choices used by the dependency traversal.
         */
        EShaderStage GetSupportedStages(
            uint32_t nodeId,
            const SMaterialGraphBindings* bindings = nullptr
        ) const;

        /** @brief Return the first emission error; empty means emission succeeded. */
        const std::string& GetDiagnostics() const { return m_Diagnostics; }

      private:
        /**
         * @brief Resolve dependencies and emit one node.
         * @param id Node to evaluate.
         * @param emitted Previously evaluated nodes.
         * @param visiting Current dependency path.
         * @param body Receives local statements.
         * @param bindings Parameter expressions and function arguments.
         * @param inlineExpression Whether to inline expressions instead of declaring locals.
         */
        SMaterialExpression EmitNode(
            uint32_t id,
            std::unordered_map<uint32_t, SMaterialExpression>& emitted,
            std::unordered_set<uint32_t>& visiting,
            std::string& body,
            const SMaterialGraphBindings* bindings,
            bool inlineExpression = false
        );

        const MaterialGraph& m_Graph;
        SMaterialEmissionEnvironment m_Environment;
        SMaterialGraphBindings m_Bindings;
        std::string m_Diagnostics;
    };
}
