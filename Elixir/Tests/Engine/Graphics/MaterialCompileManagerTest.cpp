#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialCompileManager.h>

#include <string>
#include <utility>
#include <vector>

using namespace Elixir;

namespace
{
    MaterialGraph MakeCompileGraph(const std::string& tag)
    {
        MaterialGraph graph;
        SMaterialNode custom;
        custom.Type = EMaterialNodeType::Custom;
        custom.OutputType = EGraphValueType::Float3;
        custom.Inputs = { -1, -1, -1 };
        custom.CustomCode = "float3(0.1, 0.2, 0.3) /* " + tag + " */";
        const uint32_t node = graph.AddNode(custom);
        graph.SetChannel(EMaterialChannel::BaseColor, node);
        return graph;
    }

    struct SManualScheduler
    {
        std::vector<MaterialCompileManager::Task> Tasks;

        void Enqueue(MaterialCompileManager::Task task)
        {
            Tasks.push_back(std::move(task));
        }

        void RunNext()
        {
            ASSERT_FALSE(Tasks.empty());
            MaterialCompileManager::Task task = std::move(Tasks.front());
            Tasks.erase(Tasks.begin());
            task();
        }
    };
}

TEST(MaterialCompileManager, LimitsParallelJobsAndUsesPriority)
{
    SManualScheduler scheduler;
    std::vector<std::string> resolved;
    MaterialCompileManager manager(2,
        [&](MaterialCompileManager::Task task) { scheduler.Enqueue(std::move(task)); },
        [&](const MaterialGraph& graph) -> std::optional<MaterialCompiler::SCompiled>
        {
            resolved.push_back(graph.GenerateHLSL());
            return MaterialCompiler::SCompiled{ "/tmp/material-compile-manager", "Compiled" };
        });
    const auto client = manager.CreateClient();

    manager.Request(client, 1, MakeCompileGraph("first"), {}, EMaterialCompilePriority::Normal);
    manager.Request(client, 2, MakeCompileGraph("second"), {}, EMaterialCompilePriority::Normal);
    manager.Request(client, 3, MakeCompileGraph("background"), {}, EMaterialCompilePriority::Background);
    manager.Request(client, 4, MakeCompileGraph("interactive"), {}, EMaterialCompilePriority::Interactive);

    EXPECT_EQ(manager.ActiveCount(), 2u);
    EXPECT_EQ(manager.QueuedCount(), 2u);
    EXPECT_EQ(scheduler.Tasks.size(), 2u);

    scheduler.RunNext();
    EXPECT_EQ(manager.ActiveCount(), 2u);
    EXPECT_EQ(manager.QueuedCount(), 1u);
    scheduler.RunNext();
    scheduler.RunNext();
    scheduler.RunNext();

    ASSERT_EQ(resolved.size(), 4u);
    EXPECT_NE(resolved[0].find("first"), std::string::npos);
    EXPECT_NE(resolved[1].find("second"), std::string::npos);
    EXPECT_NE(resolved[2].find("interactive"), std::string::npos);
    EXPECT_NE(resolved[3].find("background"), std::string::npos);
    EXPECT_EQ(manager.ActiveCount(), 0u);
    EXPECT_EQ(manager.QueuedCount(), 0u);
    manager.DestroyClient(client);
}

// A finished job must release its scope. If the scope stayed registered as active, the
// per-scope gate would queue every later request for that slot forever -- live preview
// and Apply would go quiet with no error.
TEST(MaterialCompileManager, AScopeCompilesAgainAfterItsJobCompletes)
{
    SManualScheduler scheduler;
    std::vector<std::string> resolved;
    MaterialCompileManager manager(2,
        [&](MaterialCompileManager::Task task) { scheduler.Enqueue(std::move(task)); },
        [&](const MaterialGraph& graph) -> std::optional<MaterialCompiler::SCompiled>
        {
            resolved.push_back(graph.GenerateHLSL());
            return MaterialCompiler::SCompiled{ "/tmp/material-compile-manager", "Compiled" };
        });
    const auto client = manager.CreateClient();

    manager.Request(client, 5, MakeCompileGraph("first-edit"), {});
    scheduler.RunNext();
    ASSERT_EQ(resolved.size(), 1u);
    EXPECT_EQ(manager.ActiveCount(), 0u);
    EXPECT_EQ(manager.State(client, 5), EMaterialCompileState::Idle);

    // The same scope again: an edit to a slot that already compiled once.
    manager.Request(client, 5, MakeCompileGraph("second-edit"), {});
    EXPECT_EQ(manager.ActiveCount(), 1u);
    ASSERT_EQ(scheduler.Tasks.size(), 1u);
    scheduler.RunNext();

    ASSERT_EQ(resolved.size(), 2u);
    EXPECT_NE(resolved[1].find("second-edit"), std::string::npos);
    EXPECT_EQ(manager.ActiveCount(), 0u);
    EXPECT_EQ(manager.QueuedCount(), 0u);
    manager.DestroyClient(client);
}

TEST(MaterialCompileManager, CoalescesQueuedRequests)
{
    SManualScheduler scheduler;
    std::vector<std::string> resolved;
    std::vector<uint64_t> completed;
    MaterialCompileManager manager(1,
        [&](MaterialCompileManager::Task task) { scheduler.Enqueue(std::move(task)); },
        [&](const MaterialGraph& graph) -> std::optional<MaterialCompiler::SCompiled>
        {
            resolved.push_back(graph.GenerateHLSL());
            return MaterialCompiler::SCompiled{ "/tmp/material-compile-manager", "Compiled" };
        });
    const auto client = manager.CreateClient();

    manager.Request(client, 99, MakeCompileGraph("blocker"), {});
    manager.Request(client, 7, MakeCompileGraph("obsolete"),
        [&](const SMaterialCompileResult& result) { completed.push_back(result.RequestId); });
    const auto latest = manager.Request(client, 7, MakeCompileGraph("latest"),
        [&](const SMaterialCompileResult& result) { completed.push_back(result.RequestId); });

    EXPECT_EQ(manager.State(client, 7), EMaterialCompileState::Queued);
    scheduler.RunNext();
    scheduler.RunNext();

    ASSERT_EQ(resolved.size(), 2u);
    EXPECT_NE(resolved[0].find("blocker"), std::string::npos);
    EXPECT_NE(resolved[1].find("latest"), std::string::npos);
    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed.front(), latest);
    EXPECT_EQ(manager.State(client, 7), EMaterialCompileState::Idle);
    manager.DestroyClient(client);
}

TEST(MaterialCompileManager, SuppressesAnInFlightStaleResult)
{
    SManualScheduler scheduler;
    std::vector<uint64_t> completed;
    MaterialCompileManager manager(2,
        [&](MaterialCompileManager::Task task) { scheduler.Enqueue(std::move(task)); },
        [](const MaterialGraph&) -> std::optional<MaterialCompiler::SCompiled>
        {
            return MaterialCompiler::SCompiled{ "/tmp/material-compile-manager", "Compiled" };
        });
    const auto client = manager.CreateClient();

    manager.Request(client, 3, MakeCompileGraph("in-flight"),
        [&](const SMaterialCompileResult& result) { completed.push_back(result.RequestId); });
    const auto latest = manager.Request(client, 3, MakeCompileGraph("replacement"),
        [&](const SMaterialCompileResult& result) { completed.push_back(result.RequestId); });

    EXPECT_EQ(manager.State(client, 3), EMaterialCompileState::Queued);
    EXPECT_EQ(scheduler.Tasks.size(), 1u);
    scheduler.RunNext();
    EXPECT_TRUE(completed.empty());
    EXPECT_EQ(manager.State(client, 3), EMaterialCompileState::Compiling);
    scheduler.RunNext();

    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed.front(), latest);
    manager.DestroyClient(client);
}

TEST(MaterialCompileManager, DestroyedClientReceivesNoCompletion)
{
    SManualScheduler scheduler;
    int completions = 0;
    MaterialCompileManager manager(1,
        [&](MaterialCompileManager::Task task) { scheduler.Enqueue(std::move(task)); },
        [](const MaterialGraph&) -> std::optional<MaterialCompiler::SCompiled>
        {
            return MaterialCompiler::SCompiled{ "/tmp/material-compile-manager", "Compiled" };
        });
    const auto client = manager.CreateClient();

    manager.Request(client, 1, MakeCompileGraph("destroyed"),
        [&](const SMaterialCompileResult&) { ++completions; });
    manager.DestroyClient(client);
    scheduler.RunNext();

    EXPECT_EQ(completions, 0);
    EXPECT_EQ(manager.ActiveCount(), 0u);
}
