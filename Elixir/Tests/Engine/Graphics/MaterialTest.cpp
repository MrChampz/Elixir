#include <gtest/gtest.h>

#include <Engine/Graphics/Material/Material.h>

using namespace Elixir;

TEST(Material, OwnsGraphAndParameterLayout)
{
    auto material = CreateRef<Material>("GraphMaterial");
    material->SetDefault("Tint", SMaterialParam::MakeVector(glm::vec4(1.0f)));
    material->SetDefault("Strength", SMaterialParam::MakeScalar(0.5f));

    MaterialGraph graph;
    SMaterialNode color;
    color.Type = EMaterialNodeType::Constant;
    color.OutputType = EGraphValueType::Float3;
    graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode(color));
    material->SetGraph(std::move(graph), {
        { "Tint", SMaterialParam::EType::Vector, 0 },
        { "Strength", SMaterialParam::EType::Scalar, 1 },
    });

    ASSERT_TRUE(material->HasGraph());
    ASSERT_NE(material->GetGraph(), nullptr);
    ASSERT_EQ(material->GetGraphParameters().size(), 2u);
    EXPECT_EQ(material->GetGraphParameters()[1].Name, "Strength");
    EXPECT_FALSE(material->GetGraph()->Validate().HasErrors());
}

TEST(MaterialInstance, PacksGraphDefaultsAndOverrides)
{
    auto material = CreateRef<Material>("GraphMaterial");
    material->SetDefault("Tint", SMaterialParam::MakeVector(glm::vec4(0.1f, 0.2f, 0.3f, 1.0f)));
    material->SetDefault("Strength", SMaterialParam::MakeScalar(0.5f));
    material->SetGraph(MaterialGraph{}, {
        { "Tint", SMaterialParam::EType::Vector, 0 },
        { "Strength", SMaterialParam::EType::Scalar, 1 },
    });

    MaterialInstance instance(material);
    instance.SetScalar("Strength", 0.8f);

    glm::vec4 params[4] = {};
    EXPECT_EQ(instance.CollectGraphParams(params, 4), 2u);
    EXPECT_EQ(params[0], glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    EXPECT_FLOAT_EQ(params[1].x, 0.8f);
}

TEST(MaterialInstance, ReparentKeepsOnlyCompatibleOverrides)
{
    auto first = CreateRef<Material>("First");
    first->SetDefault("Shared", SMaterialParam::MakeScalar(0.25f));
    first->SetDefault("Removed", SMaterialParam::MakeScalar(1.0f));
    MaterialInstance instance(first);
    instance.SetScalar("Shared", 0.75f);
    instance.SetScalar("Removed", 2.0f);

    auto second = CreateRef<Material>("Second");
    second->SetDefault("Shared", SMaterialParam::MakeScalar(0.5f));
    instance.SetParent(second);

    EXPECT_FLOAT_EQ(instance.GetScalar("Shared"), 0.75f);
    EXPECT_EQ(instance.GetParameter("Removed"), nullptr);
}

TEST(MaterialInstance, AppliesOnlyCompatiblePreparedOverrides)
{
    auto material = CreateRef<Material>("GraphMaterial");
    material->SetDefault("Strength", SMaterialParam::MakeScalar(0.5f));
    material->SetDefault("Tint", SMaterialParam::MakeVector(glm::vec4(1.0f)));

    MaterialInstance target(material);
    MaterialInstance prepared(material);
    prepared.SetScalar("Strength", 0.8f);
    prepared.SetScalar("Tint", 0.25f);
    prepared.SetScalar("Unknown", 1.0f);

    target.ApplyCompatibleOverridesFrom(prepared);

    EXPECT_FLOAT_EQ(target.GetScalar("Strength"), 0.8f);
    EXPECT_EQ(target.GetVector("Tint"), glm::vec4(1.0f));
    EXPECT_EQ(target.GetParameter("Unknown"), nullptr);
}
