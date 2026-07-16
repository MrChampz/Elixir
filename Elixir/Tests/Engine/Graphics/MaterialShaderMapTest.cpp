#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialShaderMap.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <thread>
#include <vector>

using namespace Elixir;

namespace
{
    MaterialGraph MakeKeyGraph(bool reverseOutputs = false, float value = 0.25f)
    {
        MaterialGraph graph;
        SMaterialNode constant;
        constant.Type = EMaterialNodeType::Constant;
        constant.OutputType = EGraphValueType::Float4;
        constant.ConstantValue = glm::vec4(value);
        const uint32_t node = graph.AddNode(constant);
        if (reverseOutputs)
        {
            graph.SetChannel(EMaterialChannel::Opacity, node);
            graph.SetChannel(EMaterialChannel::BaseColor, node);
        }
        else
        {
            graph.SetChannel(EMaterialChannel::BaseColor, node);
            graph.SetChannel(EMaterialChannel::Opacity, node);
        }
        return graph;
    }
}

TEST(MaterialShaderMap, ContentKeyIsStableAndCoversSources)
{
    const std::string pixelTemplate = "// __SHADING_MODEL__\nvoid main(){// __GRAPH_BODY__}";
    const std::string vertexTemplate = "void main(){// __WPO_BODY__}";

    const uint64_t ordered = MaterialCompiler::BuildContentKeyFromTemplates(
        MakeKeyGraph(false), pixelTemplate, vertexTemplate);
    const uint64_t reordered = MaterialCompiler::BuildContentKeyFromTemplates(
        MakeKeyGraph(true), pixelTemplate, vertexTemplate);
    const uint64_t changedGraph = MaterialCompiler::BuildContentKeyFromTemplates(
        MakeKeyGraph(false, 0.75f), pixelTemplate, vertexTemplate);
    const uint64_t changedTemplate = MaterialCompiler::BuildContentKeyFromTemplates(
        MakeKeyGraph(false), pixelTemplate + " ", vertexTemplate);

    EXPECT_EQ(ordered, reordered);
    EXPECT_NE(ordered, changedGraph);
    EXPECT_NE(ordered, changedTemplate);
    EXPECT_EQ(MaterialCompiler::CacheName(0x2au), "GraphMat_000000000000002a");
}

TEST(MaterialShaderMap, ConcurrentRequestsCompileOneArtifact)
{
    constexpr int REQUESTS = 8;
    std::atomic<int> compileCalls = 0;
    std::promise<void> releaseCompile;
    const std::shared_future<void> compileGate = releaseCompile.get_future().share();

    MaterialShaderMap map(
        [](const MaterialGraph&) -> std::optional<uint64_t> { return 42u; },
        [&](const MaterialGraph&) -> std::optional<MaterialCompiler::SCompiled>
        {
            ++compileCalls;
            compileGate.wait();
            return MaterialCompiler::SCompiled{ "/tmp/material-shader-map", "Shared" };
        });

    std::barrier start(REQUESTS + 1);
    std::vector<std::optional<MaterialCompiler::SCompiled>> results(REQUESTS);
    std::vector<std::thread> workers;
    workers.reserve(REQUESTS);
    for (int i = 0; i < REQUESTS; ++i)
    {
        workers.emplace_back([&, i]
        {
            start.arrive_and_wait();
            results[i] = map.GetOrCompile(MakeKeyGraph());
        });
    }

    start.arrive_and_wait();
    while (compileCalls.load() == 0)
        std::this_thread::yield();
    releaseCompile.set_value();
    for (auto& worker : workers)
        worker.join();

    EXPECT_EQ(compileCalls.load(), 1);
    EXPECT_EQ(map.EntryCount(), 1u);
    for (const auto& result : results)
    {
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->Name, "Shared");
    }
}

TEST(MaterialShaderMap, FailedArtifactsCanBeRetried)
{
    int compileCalls = 0;
    MaterialShaderMap map(
        [](const MaterialGraph&) -> std::optional<uint64_t> { return 7u; },
        [&](const MaterialGraph&) -> std::optional<MaterialCompiler::SCompiled>
        {
            ++compileCalls;
            return std::nullopt;
        });

    EXPECT_FALSE(map.GetOrCompile(MakeKeyGraph()).has_value());
    EXPECT_FALSE(map.GetOrCompile(MakeKeyGraph()).has_value());
    EXPECT_EQ(compileCalls, 2);
    EXPECT_EQ(map.EntryCount(), 0u);
}

TEST(MaterialShaderMap, ContentAddressedSpirvIsReusedAcrossMaps)
{
    if (!std::getenv("ELIXIR_RUN_DXC_CACHE_TEST"))
        GTEST_SKIP() << "Set ELIXIR_RUN_DXC_CACHE_TEST from a directory containing Shaders.";

    MaterialGraph graph;
    SMaterialNode custom;
    custom.Type = EMaterialNodeType::Custom;
    custom.OutputType = EGraphValueType::Float3;
    custom.Inputs = { -1, -1, -1 };
    const auto token = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    custom.CustomCode = "float3(0.125, 0.25, 0.5) /* cache-test-" + std::to_string(token) + " */";
    const uint32_t customId = graph.AddNode(custom);
    graph.SetChannel(EMaterialChannel::BaseColor, customId);

    MaterialShaderMap firstMap;
    const auto first = firstMap.GetOrCompile(graph);
    ASSERT_TRUE(first.has_value());
    const std::filesystem::path pixelSource =
        first->LoadDir.parent_path() / (first->Name + ".src.ps.hlsl");
    ASSERT_TRUE(std::filesystem::exists(pixelSource));
    const auto firstWrite = std::filesystem::last_write_time(pixelSource);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    MaterialShaderMap secondMap;
    const auto second = secondMap.GetOrCompile(graph);
    ASSERT_TRUE(second.has_value());

    EXPECT_EQ(second->Name, first->Name);
    EXPECT_EQ(second->LoadDir, first->LoadDir);
    EXPECT_EQ(std::filesystem::last_write_time(pixelSource), firstWrite);
}
