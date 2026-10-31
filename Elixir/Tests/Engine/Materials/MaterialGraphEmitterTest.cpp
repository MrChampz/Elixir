#include "MaterialGraphTestEmission.h"

#include <gtest/gtest.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/Fresnel.h>
#include <Engine/Materials/Nodes/FunctionCall.h>
#include <Engine/Materials/Nodes/Noise.h>
#include <Engine/Materials/Nodes/Panner.h>
#include <Engine/Materials/Nodes/Position.h>
#include <Engine/Materials/Nodes/StaticSwitch.h>
#include <Engine/Materials/Nodes/TexCoord.h>
#include <Engine/Materials/Nodes/TextureSample.h>
#include <Engine/Materials/Nodes/Time.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;
using namespace Elixir::Materials::Compilation;

TEST(MaterialGraphEmitterTest, ResolvesSemanticInputsWithoutTemplateNamesInNodes)
{
    MaterialGraph graph;
    graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode<Position>());
    graph.SetChannel(EMaterialChannel::Emissive, graph.AddNode<TexCoord>(1));
    graph.SetChannel(EMaterialChannel::Metallic, graph.AddNode<Time>());
    graph.SetChannel(EMaterialChannel::Roughness, graph.AddNode<Fresnel>());
    SMaterialEmissionEnvironment environment;
    environment.WorldPosition = "customPosition";
    environment.TexCoords[1] = "customUV";
    environment.Time = "customTime";
    environment.Normal = "customNormal";
    environment.ViewDirection = "customView";
    MaterialGraphEmitter emitter(graph, environment);
    const auto source = emitter.GenerateHLSL();
    EXPECT_TRUE(emitter.GetDiagnostics().empty());
    for (const auto name :
         {"customPosition", "customUV", "customTime", "customNormal", "customView"})
        EXPECT_NE(source.find(name), std::string::npos);
    EXPECT_EQ(source.find("input."), std::string::npos);
}

TEST(MaterialGraphEmitterTest, SemanticDefaultsUseTheConfiguredCoordinatesAndTime)
{
    MaterialGraph graph;
    graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode<Panner>(glm::vec2(1)));
    graph.SetChannel(EMaterialChannel::Roughness, graph.AddNode<Noise>());
    SMaterialEmissionEnvironment environment;
    environment.TexCoords[0] = "meshUV";
    environment.Time = "elapsedSeconds";
    MaterialGraphEmitter emitter(graph, environment);
    const auto source = emitter.GenerateHLSL();
    EXPECT_TRUE(emitter.GetDiagnostics().empty());
    EXPECT_NE(source.find("meshUV"), std::string::npos);
    EXPECT_NE(source.find("elapsedSeconds"), std::string::npos);
    EXPECT_EQ(source.find("input.TexCoord"), std::string::npos);
}

TEST(MaterialGraphEmitterTest, NestedFunctionsInheritResourcesAndParameterBindings)
{
    MaterialGraph inner;
    const auto texture = inner.AddNode<TextureSample>(
        "Texture",
        ETextureSampleType::Mask,
        STextureSampleSettings{.Address = ETextureAddress::Clamp}
    );
    const auto function = CreateRef<MaterialFunction>(
        std::move(inner),
        std::vector<SMaterialFunctionInput>{},
        texture
    );
    MaterialGraph outer;
    const auto call = outer.AddNode<FunctionCall>(function);
    const auto wrapper = CreateRef<MaterialFunction>(
        std::move(outer),
        std::vector<SMaterialFunctionInput>{},
        call
    );
    MaterialGraph graph;
    graph.SetChannel(
        EMaterialChannel::WorldPositionOffset,
        graph.AddNode<FunctionCall>(wrapper)
    );
    SMaterialEmissionEnvironment environment;
    environment.Stage = EShaderStage::Vertex;
    environment.TexCoords[0] = "vertexUV";
    environment.TextureArray = "images";
    environment.TextureSamplers[1] = "clampSampler";
    MaterialGraphEmitter emitter(
        graph,
        environment,
        {.Textures = {{"Texture", "indices[4]"}}}
    );
    const auto source = emitter.GenerateHLSL();
    EXPECT_TRUE(emitter.GetDiagnostics().empty());
    EXPECT_NE(
        source.find("images[indices[4]].SampleLevel(clampSampler, vertexUV, 0.0)"),
        std::string::npos
    );
    EXPECT_EQ(source.find("textures["), std::string::npos);
}

TEST(MaterialGraphEmitterTest, ReportsMissingInputsInNestedFunctions)
{
    MaterialGraph inner;
    const auto position = inner.AddNode<Position>();
    const auto function = CreateRef<MaterialFunction>(
        std::move(inner),
        std::vector<SMaterialFunctionInput>{},
        position
    );
    MaterialGraph graph;
    graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode<FunctionCall>(function));
    MaterialGraphEmitter emitter(graph, {});
    EXPECT_TRUE(emitter.GenerateHLSL().empty());
    EXPECT_NE(emitter.GetDiagnostics().find("world position"), std::string::npos);
}

TEST(MaterialGraphEmitterTest, ConnectedUVDoesNotRequireATemplateDefault)
{
    MaterialGraph graph;
    const auto uv = graph.AddNode<Constant>(glm::vec4(.5f), EMaterialValueType::Float2);
    const auto texture = graph.AddNode<TextureSample>("Texture");
    graph.Connect(uv, texture, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, texture);
    SMaterialEmissionEnvironment environment;
    environment.TextureArray = "images";
    environment.TextureSamplers[0] = "sampler0";
    MaterialGraphEmitter emitter(graph, environment);
    EXPECT_FALSE(emitter.GenerateHLSL().empty());
    EXPECT_TRUE(emitter.GetDiagnostics().empty());
}

TEST(MaterialGraphEmitterTest, UnselectedBranchesDoNotRequireTheirShaderInputs)
{
    MaterialGraph graph;
    const auto unavailable = graph.AddNode<Position>();
    const auto available = graph.AddNode<Constant>(glm::vec4(1), EMaterialValueType::Float);
    const auto choice = graph.AddNode<StaticSwitch>("Position", true);
    graph.Connect(unavailable, choice, 0);
    graph.Connect(available, choice, 1);
    graph.SetChannel(EMaterialChannel::Metallic, choice);
    MaterialGraphEmitter emitter(graph, {}, {.StaticValues = {{"Position", false}}});
    EXPECT_FALSE(emitter.GenerateHLSL().empty());
    EXPECT_TRUE(emitter.GetDiagnostics().empty());
}

TEST(MaterialGraphEmitterTest, ReportsUnavailableSamplersAndCoordinateChannels)
{
    MaterialGraph graph;
    const auto texture = graph.AddNode<TextureSample>("Texture");
    graph.SetChannel(EMaterialChannel::BaseColor, texture);
    SMaterialEmissionEnvironment environment;
    environment.TextureArray = "images";
    environment.TexCoords[0] = "uv";
    MaterialGraphEmitter emitter(graph, environment);
    EXPECT_TRUE(emitter.GenerateHLSL().empty());
    EXPECT_NE(emitter.GetDiagnostics().find("sampler"), std::string::npos);

    MaterialGraph invalidUV;
    invalidUV.SetChannel(EMaterialChannel::BaseColor, invalidUV.AddNode<TexCoord>(2));
    MaterialGraphEmitter invalid(invalidUV, environment);
    EXPECT_TRUE(invalid.GenerateHLSL().empty());
    EXPECT_NE(invalid.GetDiagnostics().find("coordinate channel"), std::string::npos);
}

TEST(MaterialGraphEmitterTest, SourceBuilderOwnsTemplateDefinesAndRejectsMissingInputs)
{
    const auto material = CreateRef<Material>("Source");
    MaterialGraph graph;
    const auto position = graph.AddNode<Position>();
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, position);
    material->SetGraph(std::move(graph));
    const auto compiled = Compiler::Build(*material);
    ASSERT_TRUE(compiled);
    const auto vertex = MaterialShaderSourceBuilder::Build(
        "// shader template\n// __GRAPH_BODY__",
        *material,
        *compiled.Material,
        EShaderStage::Vertex
    );
    ASSERT_TRUE(vertex) << vertex.Diagnostics;
    EXPECT_TRUE(vertex.Diagnostics.empty());
    EXPECT_NE(vertex.Source.find("#define GRAPH_HAS_WPO 1"), std::string::npos);
    const auto pixel = MaterialShaderSourceBuilder::Build(
        "// shader template\n// __GRAPH_BODY__",
        *material,
        *compiled.Material,
        EShaderStage::Pixel
    );
    ASSERT_TRUE(pixel) << pixel.Diagnostics;
    EXPECT_TRUE(pixel.Diagnostics.empty());
    EXPECT_EQ(pixel.Source.find("GRAPH_HAS_WPO"), std::string::npos);
    const auto invalid = MaterialShaderSourceBuilder::Build(
        "no marker",
        *material,
        *compiled.Material,
        EShaderStage::Pixel
    );
    EXPECT_FALSE(invalid);
    EXPECT_TRUE(invalid.Source.empty());
    EXPECT_FALSE(invalid.Diagnostics.empty());
}

TEST(MaterialGraphEmitterTest, SourceBuilderDisablesDisplacementForGraphsWithoutThatOutput)
{
    const auto material = CreateRef<Material>("NoDisplacement");
    MaterialGraph graph;
    const auto color = graph.AddNode<Constant>(glm::vec4(1), EMaterialValueType::Float3);
    graph.SetChannel(EMaterialChannel::BaseColor, color);
    material->SetGraph(std::move(graph));
    const auto compiled = Compiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto vertex = MaterialShaderSourceBuilder::Build(
        "// __GRAPH_BODY__",
        *material,
        *compiled.Material,
        EShaderStage::Vertex
    );
    ASSERT_TRUE(vertex) << vertex.Diagnostics;
    EXPECT_NE(vertex.Source.find("#define GRAPH_HAS_WPO 0\n"), std::string::npos);

    const auto pixel = MaterialShaderSourceBuilder::Build(
        "// __GRAPH_BODY__",
        *material,
        *compiled.Material,
        EShaderStage::Pixel
    );
    ASSERT_TRUE(pixel) << pixel.Diagnostics;
    EXPECT_EQ(pixel.Source.find("GRAPH_HAS_WPO"), std::string::npos);
}
