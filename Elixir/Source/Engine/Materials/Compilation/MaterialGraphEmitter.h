#pragma once

#include <Engine/Materials/MaterialGraph.h>
#include <Engine/Materials/Compilation/MaterialEmissionEnvironment.h>
#include <Engine/Materials/Compilation/MaterialGraphBindings.h>

namespace Elixir::Materials { class MaterialFunction; }

namespace Elixir::Materials::Compilation
{
    /** Stores an expression and the local statements required to evaluate it. */
    struct SMaterialExpressionSource
    {
        /** Expression evaluated after Statements have run. */
        SMaterialExpression Expression;

        /** Local declarations emitted once for each shared dependency. */
        std::string Statements;
    };

    /** @brief Traverse a material graph for validation and shader code emission. */
    class ELIXIR_API MaterialGraphEmitter final
    {
        friend class MaterialEmitContext;

      public:
        /**
         * @brief Bind an immutable graph to one shader environment.
         * @param graph Graph kept alive throughout emission.
         * @param environment Available shader inputs and resources.
         * @param bindings Parameter expressions and compile-time arguments.
         * @param localPrefix HLSL identifier prefix unique to this function call.
         */
        MaterialGraphEmitter(
            const MaterialGraph& graph,
            const SMaterialEmissionEnvironment& environment,
            const SMaterialGraphBindings& bindings = {},
            std::string localPrefix = "n"
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
         * @brief Generate one output and the local statements needed to evaluate it.
         * @param nodeId Output node to evaluate.
         * @return Output expression and statements to insert before it in the same scope.
         */
        SMaterialExpressionSource GenerateExpression(uint32_t nodeId);

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
         */
        SMaterialExpression EmitNode(
            uint32_t id,
            std::unordered_map<uint32_t, SMaterialExpression>& emitted,
            std::unordered_set<uint32_t>& visiting
        );

        /**
         * @brief Emit a function call and append its local declarations.
         * @param function Immutable function to evaluate.
         * @param inputs Resolved arguments in the function's declared order.
         */
        SMaterialExpression CallFunction(
            const MaterialFunction& function,
            const std::vector<SMaterialExpression>& inputs
        );

        const MaterialGraph& m_Graph;
        SMaterialEmissionEnvironment m_Environment;
        SMaterialGraphBindings m_Bindings;
        std::string m_Statements;
        uint32_t m_FunctionCallCount = 0;
        std::string m_LocalPrefix;
        std::string m_Diagnostics;
    };
}
