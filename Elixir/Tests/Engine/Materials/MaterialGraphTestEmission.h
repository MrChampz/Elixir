#pragma once

#include <Engine/Materials/Compilation/MaterialGraphEmitter.h>
#include <Engine/Materials/Compilation/MaterialShaderSourceBuilder.h>

namespace Elixir::Materials::Tests
{
    /**
     * @brief Emit a graph using one of the engine's existing shader environments.
     * @param graph Graph under test.
     * @param bindings Parameter and static-choice expressions.
     * @param stage Concrete shader stage.
     * @param usage Template renderer contract.
     */
    inline std::string EmitGraph(
        const MaterialGraph& graph,
        const Compilation::SMaterialGraphBindings& bindings = {},
        EShaderStage stage = EShaderStage::Pixel,
        EMaterialUsage usage = EMaterialUsage::Surface
    )
    {
        const auto environment =
            Compilation::MaterialShaderSourceBuilder::CreateEnvironment(usage, stage);
        return Compilation::MaterialGraphEmitter(graph, environment, bindings).GenerateHLSL();
    }

    /**
     * @brief Emit an expression with explicit inputs available in every tested stage.
     * @param graph Graph under test.
     * @param output Expression output.
     * @param bindings Parameter expressions.
     * @param stage Concrete stage for expression emission.
     */
    inline SMaterialExpression EmitExpression(
        const MaterialGraph& graph,
        uint32_t output,
        const Compilation::SMaterialGraphBindings& bindings,
        EShaderStage stage
    )
    {
        auto environment = Compilation::MaterialShaderSourceBuilder::CreateEnvironment(
            EMaterialUsage::Surface,
            stage == EShaderStage::Vertex ? EShaderStage::Vertex : EShaderStage::Pixel
        );
        environment.Stage = stage;
        return Compilation::MaterialGraphEmitter(graph, environment, bindings)
            .GenerateExpression(output);
    }

    /**
     * @brief Inspect active dependency stage restrictions without emitting a shader.
     * @param graph Graph under test.
     * @param output Output to inspect.
     * @param bindings Compile-time choices for the traversal.
     */
    inline EShaderStage SupportedStages(
        const MaterialGraph& graph,
        uint32_t output,
        const Compilation::SMaterialGraphBindings* bindings = nullptr
    )
    {
        const Compilation::MaterialGraphEmitter emitter(graph, {});
        return emitter.GetSupportedStages(output, bindings);
    }
}
