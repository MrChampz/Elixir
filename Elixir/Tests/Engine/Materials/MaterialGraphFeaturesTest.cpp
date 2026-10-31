#include "MaterialGraphTestEmission.h"
#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/CompilationCache.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/Nodes/Add.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/Custom.h>
#include <Engine/Materials/Nodes/FunctionCall.h>
#include <Engine/Materials/Nodes/FunctionInput.h>
#include <Engine/Materials/Nodes/Noise.h>
#include <Engine/Materials/Nodes/Parameter.h>
#include <Engine/Materials/Nodes/Position.h>
#include <Engine/Materials/Nodes/StaticSwitch.h>
#include <Engine/Materials/Nodes/StaticBoolParameter.h>
#include <Engine/Materials/Nodes/TextureSample.h>

using namespace Elixir;
using namespace Elixir::Materials::Tests;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;
using namespace Elixir::Materials::Compilation;

namespace
{
    Ref<const MaterialFunction> MakeFunction()
    {
        MaterialGraph graph;
        const auto input = graph.AddNode<FunctionInput>("Value", EMaterialValueType::Float);
        const auto offset = graph.AddNode<Parameter>("Offset", EMaterialValueType::Float);
        const auto add = graph.AddNode<Add>();
        graph.Connect(input, add, 0);
        graph.Connect(offset, add, 1);
        return CreateRef<MaterialFunction>(
            std::move(graph),
            std::vector<SMaterialFunctionInput>{
                {"Value", EMaterialValueType::Float, glm::vec4(2)}
            },
            add
        );
    }

    void DefineOffset(const Ref<Material>& material)
    {
        ASSERT_TRUE(material->DefineParameter(
            "Offset",
            {.Kind = EMaterialParameterKind::Value,
             .ValueType = EMaterialValueType::Float,
             .DefaultValue = SMaterialParameter::MakeScalar(1)}
        ));
    }
}

TEST(MaterialGraphFeaturesTest, NestedFunctionsInheritBindingsAndKeepArgumentsLocal)
{
    const auto function = MakeFunction();
    MaterialGraph outer;
    const auto value = outer.AddNode<FunctionInput>("Outer", EMaterialValueType::Float);
    const auto nested = outer.AddNode<FunctionCall>(function);
    outer.Connect(value, nested, 0);
    const auto wrapper = CreateRef<MaterialFunction>(
        std::move(outer),
        std::vector<SMaterialFunctionInput>{
            {"Outer", EMaterialValueType::Float, glm::vec4(5)}
        },
        nested
    );
    MaterialGraph graph;
    const auto first = graph.AddNode<FunctionCall>(wrapper);
    const auto second = graph.AddNode<FunctionCall>(function);
    graph.SetChannel(EMaterialChannel::Metallic, first);
    graph.SetChannel(EMaterialChannel::Roughness, second);
    const auto material = CreateRef<Material>("Functions");
    material->SetGraph(std::move(graph));
    DefineOffset(material);
    ASSERT_TRUE(Compiler::Build(*material));
    const auto source =
        EmitGraph(material->GetGraph(), {.Values = {{"Offset", "mat.Values[3].x"}}});
    EXPECT_NE(source.find("5.000000"), std::string::npos);
    EXPECT_NE(source.find("2.000000"), std::string::npos);
    EXPECT_NE(source.find("mat.Values[3].x"), std::string::npos);
    EXPECT_EQ(source.find("mat.Offset"), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, ConnectedFunctionInputsOverrideDefaults)
{
    MaterialGraph graph;
    const auto value = graph.AddNode<Constant>(glm::vec4(7), EMaterialValueType::Float);
    const auto call = graph.AddNode<FunctionCall>(MakeFunction());
    graph.Connect(value, call, 0);
    graph.SetChannel(EMaterialChannel::Metallic, call);
    const auto source = EmitGraph(graph);
    EXPECT_NE(source.find("7.000000"), std::string::npos);
    EXPECT_EQ(source.find("2.000000"), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, RejectsMissingFunctionAndUnboundFunctionInputs)
{
    MaterialGraph graph;
    graph.AddNode<FunctionCall>(nullptr);
    auto material = CreateRef<Material>("Invalid");
    material->SetGraph(std::move(graph));
    EXPECT_FALSE(Compiler::Build(*material));
    MaterialGraph unbound;
    unbound.AddNode<FunctionInput>("Unknown", EMaterialValueType::Float);
    material->SetGraph(std::move(unbound));
    EXPECT_FALSE(Compiler::Build(*material));
}

TEST(MaterialGraphFeaturesTest, FunctionValidatesItsParameterReferencesInCallerSchema)
{
    MaterialGraph graph;
    const auto call = graph.AddNode<FunctionCall>(MakeFunction());
    graph.SetChannel(EMaterialChannel::Roughness, call);
    const auto material = CreateRef<Material>("MissingParameter");
    material->SetGraph(std::move(graph));
    EXPECT_FALSE(Compiler::Build(*material));
    DefineOffset(material);
    EXPECT_TRUE(Compiler::Build(*material));
}

TEST(MaterialGraphFeaturesTest, StaticSwitchOmitsTheUnselectedBranch)
{
    MaterialGraph graph;
    const auto first = graph.AddNode<Custom>("123.0", EMaterialValueType::Float);
    const auto second = graph.AddNode<Custom>("456.0", EMaterialValueType::Float);
    const auto choice = graph.AddNode<StaticSwitch>("Feature", true);
    graph.Connect(first, choice, 0);
    graph.Connect(second, choice, 1);
    graph.SetChannel(EMaterialChannel::Metallic, choice);
    const auto defaults = EmitGraph(graph);
    const auto overridden = EmitGraph(graph, {.StaticValues = {{"Feature", false}}});
    EXPECT_NE(defaults.find("123.0"), std::string::npos);
    EXPECT_EQ(defaults.find("456.0"), std::string::npos);
    EXPECT_EQ(overridden.find("123.0"), std::string::npos);
    EXPECT_NE(overridden.find("456.0"), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, StaticVariantsAreSharedOnlyForMatchingChoices)
{
    const auto material = CreateRef<Material>("Variants");
    CompilationCache cache(nullptr);
    const auto first = cache.GetOrCompile(material, {{"Feature", true}});
    const auto second = cache.GetOrCompile(material, {{"Feature", false}});
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_NE(first, second);
    EXPECT_EQ(first, cache.GetOrCompile(material, {{"Feature", true}}));
    const auto instance = material->CreateInstance();
    const auto revision = instance->GetRevision();
    ASSERT_TRUE(instance->SetStaticBool("Feature", true));
    EXPECT_GT(instance->GetRevision(), revision);
    EXPECT_EQ(instance->GetStaticValues(), first->StaticValues);
    EXPECT_FALSE(instance->SetStaticBool("", false));
}

TEST(MaterialGraphFeaturesTest, WorldPositionOffsetIsGeneratedOnlyForTheVertexStage)
{
    MaterialGraph graph;
    const auto position = graph.AddNode<Position>();
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, position);
    graph.SetChannel(EMaterialChannel::BaseColor, position);
    const auto pixel = EmitGraph(graph);
    const auto vertex = EmitGraph(graph, {}, EShaderStage::Vertex);
    EXPECT_NE(pixel.find("input.WorldPos"), std::string::npos);
    EXPECT_EQ(pixel.find("worldPositionOffset"), std::string::npos);
    EXPECT_NE(vertex.find("worldPos"), std::string::npos);
    EXPECT_NE(vertex.find("worldPositionOffset ="), std::string::npos);
    EXPECT_EQ(vertex.find("surface.BaseColor"), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, RejectsPixelDerivativesInVertexOutputsIncludingFunctions)
{
    MaterialGraph functionGraph;
    const auto derivative =
        functionGraph.AddNode<Custom>("ddx(1.0)", EMaterialValueType::Float);
    const auto function = CreateRef<MaterialFunction>(
        std::move(functionGraph),
        std::vector<SMaterialFunctionInput>{},
        derivative
    );
    MaterialGraph graph;
    const auto call = graph.AddNode<FunctionCall>(function);
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, call);
    const auto material = CreateRef<Material>("Vertex");
    material->SetGraph(std::move(graph));
    EXPECT_FALSE(Compiler::Build(*material));
}

TEST(MaterialGraphFeaturesTest, VertexValidationUsesTheSelectedStaticBranch)
{
    MaterialGraph graph;
    const auto derivative = graph.AddNode<Custom>("ddx(1.0)", EMaterialValueType::Float);
    const auto safe = graph.AddNode<Constant>(glm::vec4(0), EMaterialValueType::Float);
    const auto choice = graph.AddNode<StaticSwitch>("Derivatives", true);
    graph.Connect(derivative, choice, 0);
    graph.Connect(safe, choice, 1);
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, choice);
    const auto material = CreateRef<Material>("VertexChoice");
    material->SetGraph(std::move(graph));
    EXPECT_FALSE(Compiler::Build(*material));
    EXPECT_TRUE(Compiler::Build(*material, {{"Derivatives", false}}));
}

TEST(MaterialGraphFeaturesTest, RejectsWorldPositionOffsetForParticleUsage)
{
    MaterialGraph graph;
    const auto zero = graph.AddNode<Constant>(glm::vec4(0), EMaterialValueType::Float3);
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, zero);
    const auto material = CreateRef<Material>("Particle", EMaterialUsage::Particle);
    material->SetGraph(std::move(graph));
    EXPECT_FALSE(Compiler::Build(*material));
}

TEST(MaterialGraphFeaturesTest, TextureSettingsControlSamplerMipAndOutputType)
{
    MaterialGraph graph;
    const auto texture = graph.AddNode<TextureSample>(
        "Texture",
        ETextureSampleType::Mask,
        STextureSampleSettings{
            .Address = ETextureSampleAddress::Clamp,
            .Filter = ETextureSampleFilter::Point,
            .MipMode = ETextureSampleMipMode::Level,
            .Output = ETextureSampleOutput::A
        }
    );
    graph.SetChannel(EMaterialChannel::Opacity, texture);
    const auto source =
        EmitGraph(graph, {.Textures = {{"Texture", "mat.TextureIndices[2]"}}});
    EXPECT_NE(source.find(".SampleLevel(pointClamp"), std::string::npos);
    EXPECT_NE(source.find(".a"), std::string::npos);
    EXPECT_NE(source.find("float n"), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, VertexTexturesUseExplicitLodAndNoiseEmitsSmoothInterpolation)
{
    MaterialGraph graph;
    const auto texture = graph.AddNode<TextureSample>("Texture");
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, texture);
    const auto vertex = EmitGraph(graph, {}, EShaderStage::Vertex);
    EXPECT_NE(vertex.find(".SampleLevel("), std::string::npos);
    EXPECT_EQ(vertex.find(".Sample("), std::string::npos);
    MaterialGraph noiseGraph;
    const auto noise = noiseGraph.AddNode<Noise>();
    noiseGraph.SetChannel(EMaterialChannel::Roughness, noise);
    const auto source = EmitGraph(noiseGraph);
    EXPECT_NE(source.find("lerp(lerp("), std::string::npos);
    EXPECT_NE(source.find("floor("), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, RejectsDuplicateFunctionArgumentsAndMissingOutputs)
{
    for (const bool duplicate : {false, true})
    {
        MaterialGraph functionGraph;
        const auto value =
            functionGraph.AddNode<Constant>(glm::vec4(1), EMaterialValueType::Float);
        auto inputs = std::vector<SMaterialFunctionInput>{{"Value"}, {"Value"}};
        if (!duplicate)
            inputs.clear();
        const auto function = CreateRef<MaterialFunction>(
            std::move(functionGraph),
            std::move(inputs),
            duplicate ? value : value + 1
        );
        MaterialGraph graph;
        graph.AddNode<FunctionCall>(function);
        const auto material = CreateRef<Material>("InvalidFunction");
        material->SetGraph(std::move(graph));
        EXPECT_FALSE(Compiler::Build(*material));
    }
}

TEST(MaterialGraphFeaturesTest, RejectsDanglingConnectionsInFunctionGraphs)
{
    MaterialGraph functionGraph;
    const auto output = functionGraph.AddNode<Add>();
    functionGraph.Connect(999, output, 0);
    const auto function = CreateRef<MaterialFunction>(
        std::move(functionGraph),
        std::vector<SMaterialFunctionInput>{},
        output
    );
    MaterialGraph graph;
    graph.AddNode<FunctionCall>(function);
    const auto material = CreateRef<Material>("Dangling");
    material->SetGraph(std::move(graph));
    EXPECT_FALSE(Compiler::Build(*material));
}

TEST(MaterialGraphFeaturesTest, StaticVariantsInvalidateAfterParentChanges)
{
    const auto material = CreateRef<Material>("Invalidation");
    CompilationCache cache(nullptr);
    const auto oldTrue = cache.GetOrCompile(material, {{"Choice", true}});
    const auto oldFalse = cache.GetOrCompile(material, {{"Choice", false}});
    material->SetGraph(MaterialGraph{});
    EXPECT_NE(oldTrue, cache.GetOrCompile(material, {{"Choice", true}}));
    EXPECT_NE(oldFalse, cache.GetOrCompile(material, {{"Choice", false}}));
}

TEST(MaterialGraphFeaturesTest, CustomExpressionsConvertTypedInputsAndStaticBooleans)
{
    MaterialGraph graph;
    const auto choice = graph.AddNode<StaticBoolParameter>("Enabled", false);
    const auto custom = graph.AddNode<Custom>(
        "sin({0}) + {0}",
        EMaterialValueType::Float,
        std::vector<SMaterialNodeInput>{
            {"Value", EMaterialValueType::Float, "0.0", EMaterialValueType::Float}
        },
        EShaderStage::All
    );
    graph.Connect(choice, custom, 0);
    graph.SetChannel(EMaterialChannel::Roughness, custom);
    const auto source = EmitGraph(graph, {.StaticValues = {{"Enabled", true}}});
    EXPECT_NE(source.find("= 1.0;"), std::string::npos);
    EXPECT_NE(source.find("sin((n"), std::string::npos);
    EXPECT_EQ(source.find("{0}"), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, SupportedStagesIntersectDependenciesAndRespectStaticChoices)
{
    MaterialGraph graph;
    const auto pixel = graph.AddNode<Custom>("ddx(1.0)", EMaterialValueType::Float);
    const auto value = graph.AddNode<Constant>(glm::vec4(0), EMaterialValueType::Float);
    const auto choice = graph.AddNode<StaticSwitch>("PixelOnly", true);
    graph.Connect(pixel, choice, 0);
    graph.Connect(value, choice, 1);
    EXPECT_EQ(SupportedStages(graph, choice), EShaderStage::Pixel);
    SMaterialGraphBindings bindings;
    bindings.StaticValues["PixelOnly"] = false;
    EXPECT_EQ(SupportedStages(graph, choice, &bindings), EShaderStage::All);
}

TEST(MaterialGraphFeaturesTest, NestedFunctionsKeepTheEmissionStageOutsideParameterBindings)
{
    MaterialGraph functionGraph;
    const auto position = functionGraph.AddNode<Position>();
    const auto function = CreateRef<MaterialFunction>(
        std::move(functionGraph),
        std::vector<SMaterialFunctionInput>{},
        position
    );
    MaterialGraph outerGraph;
    const auto nested = outerGraph.AddNode<FunctionCall>(function);
    const auto outer = CreateRef<MaterialFunction>(
        std::move(outerGraph),
        std::vector<SMaterialFunctionInput>{},
        nested
    );
    EXPECT_EQ(outer->GetSupportedStages(), EShaderStage::Vertex | EShaderStage::Pixel);
    MaterialGraph graph;
    const auto call = graph.AddNode<FunctionCall>(outer);
    const auto vertex = EmitExpression(graph, call, {}, EShaderStage::Vertex);
    const auto pixel = EmitExpression(graph, call, {}, EShaderStage::Pixel);
    EXPECT_NE(vertex.Code.find("worldPos"), std::string::npos);
    EXPECT_EQ(vertex.Code.find("input.WorldPos"), std::string::npos);
    EXPECT_NE(pixel.Code.find("input.WorldPos"), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, ConcreteStagesRejectMasksAndSentinels)
{
    for (const auto stage :
         {EShaderStage::Vertex,
          EShaderStage::Hull,
          EShaderStage::Domain,
          EShaderStage::Geometry,
          EShaderStage::Pixel,
          EShaderStage::Compute})
        EXPECT_TRUE(MaterialEmitContext::IsValidStage(stage));
    for (const auto stage :
         {EShaderStage::None,
          EShaderStage::Count,
          EShaderStage::Graphics,
          EShaderStage::All,
          EShaderStage::Vertex | EShaderStage::Pixel})
        EXPECT_FALSE(MaterialEmitContext::IsValidStage(stage));

    MaterialGraph graph;
    const auto value = graph.AddNode<Constant>(glm::vec4(1), EMaterialValueType::Float);
    graph.SetChannel(EMaterialChannel::Metallic, value);
    EXPECT_TRUE(EmitGraph(graph, {}, EShaderStage::Geometry).empty());
    EXPECT_TRUE(EmitGraph(graph, {}, EShaderStage::Vertex | EShaderStage::Pixel).empty());
}

TEST(MaterialGraphFeaturesTest, NonPixelTextureExpressionsUseExplicitLod)
{
    MaterialGraph graph;
    const auto texture = graph.AddNode<TextureSample>("Texture");
    for (const auto stage : {EShaderStage::Geometry, EShaderStage::Compute})
    {
        const auto expression = EmitExpression(graph, texture, {}, stage);
        EXPECT_NE(expression.Code.find(".SampleLevel("), std::string::npos);
        EXPECT_EQ(expression.Code.find(".Sample("), std::string::npos);
    }
    const auto pixel = EmitExpression(graph, texture, {}, EShaderStage::Pixel);
    EXPECT_NE(pixel.Code.find(".Sample("), std::string::npos);
}

TEST(MaterialGraphFeaturesTest, CustomExpressionsDeclareStageMasks)
{
    MaterialGraph graph;
    const auto stages = EShaderStage::Geometry | EShaderStage::Compute;
    const auto custom = graph.AddNode<Custom>(
        "42.0",
        EMaterialValueType::Float,
        std::vector<SMaterialNodeInput>{},
        stages
    );
    EXPECT_EQ(SupportedStages(graph, custom), stages);
    const auto compute = EmitExpression(graph, custom, {}, EShaderStage::Compute);
    EXPECT_NE(compute.Code.find("42.0"), std::string::npos);
    const auto pixel = EmitExpression(graph, custom, {}, EShaderStage::Pixel);
    EXPECT_EQ(pixel.Code, "0.0");
    graph.SetChannel(EMaterialChannel::Metallic, custom);
    const auto material = CreateRef<Material>("InvalidPixelStage");
    material->SetGraph(std::move(graph));
    EXPECT_FALSE(Compiler::Build(*material));
}
