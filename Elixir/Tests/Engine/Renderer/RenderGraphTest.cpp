#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Core/Executor.h>
#include <Engine/Renderer/RenderGraph.h>
using namespace Elixir;
using namespace Elixir::Renderer;

class RenderGraphTest : public Test
{
  protected:
    void SetUp() override
    {
        m_Executor = CreateScope<Executor>();
        m_Executor->Init();
    }

    Scope<Executor> m_Executor;
};

TEST_F(RenderGraphTest, PassExecutionOrder)
{
    std::vector<std::string> logs;

    RenderGraph rg(m_Executor.get());

    auto tex1 = rg.CreateResource({ ERGResourceType::Texture, 900, 600 });
    auto tex2 = rg.CreateResource({ ERGResourceType::Texture, 900, 600 });
    rg.MarkExternalOutput(tex1);
    rg.MarkExternalOutput(tex2);

    rg.AddPass("Pass 1", {}, { tex1 }, [&]()
    {
        logs.push_back("Pass 1");
    });

    rg.AddPass("Pass 2", { tex2 }, {}, [&]()
    {
        logs.push_back("Pass 2");
    });

    rg.AddPass("Pass 3", { tex1 }, { tex2 }, [&]()
    {
        logs.push_back("Pass 3");
    });

    rg.Compile();
    rg.Execute();

    EXPECT_EQ(logs.size(), 2u);
    EXPECT_EQ(logs[0], "Pass 1");
    EXPECT_EQ(logs[1], "Pass 3");
}

TEST_F(RenderGraphTest, CullUnusedPasses)
{
    std::vector<std::string> logs;

    RenderGraph rg(m_Executor.get());

    auto tex1 = rg.CreateResource({ ERGResourceType::Texture, 900, 600 });
    auto tex2 = rg.CreateResource({ ERGResourceType::Texture, 900, 600 });
    rg.MarkExternalOutput(tex2);

    rg.AddPass("Pass 1", {}, { tex1 }, [&]()
    {
        logs.push_back("Pass 1");
    });

    rg.AddPass("Pass 2", { tex2 }, {}, [&]()
    {
        logs.push_back("Pass 2");
    });

    rg.AddPass("Pass 3", {}, {}, [&]()
    {
        logs.push_back("Pass 3");
    });

    rg.AddPass("Pass 4", { tex1 }, { tex2 }, [&]()
    {
        logs.push_back("Pass 4");
    });

    rg.AddPass("Pass 5", {}, {}, [&]()
    {
        logs.push_back("Pass 5");
    });

    rg.Compile();
    rg.Execute();

    EXPECT_EQ(logs.size(), 2u);
    EXPECT_EQ(logs[0], "Pass 1");
    EXPECT_EQ(logs[1], "Pass 4");
}

TEST_F(RenderGraphTest, ThrowsErrorIfNotCompiled)
{
    RenderGraph rg(m_Executor.get());

    rg.AddPass("Pass 1", {}, {}, [&](){});
    rg.AddPass("Pass 2", {}, {}, [&](){});
    rg.AddPass("Pass 3", {}, {}, [&](){});

    EXPECT_THROW(rg.Execute(), std::runtime_error);
}