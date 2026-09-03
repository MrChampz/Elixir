#include <gtest/gtest.h>

#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/Nodes/Parameter.h>
#include <Engine/Materials/Nodes/TextureSample.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;

TEST(MaterialTest, ValidateGraphParametersAgainstMaterialSchema)
{
    MaterialGraph graph;

    graph.SetChannel(
        EMaterialChannel::BaseColor,
        graph.AddNode<Parameter>("Tint", EMaterialValueType::Float4)
    );

    auto material = CreateRef<Material>("Tinted");
    material->SetGraph(std::move(graph));

    EXPECT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector(glm::vec4{ 1.0f }),
    }));
    EXPECT_TRUE(material->ValidateGraph());
}

TEST(MaterialTest, RejectsOverridesThatDoNotMatchTheSchema)
{
    const auto material = CreateRef<Material>("Tinted");

    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector(glm::vec4{ 1.0f }),
    }));

    MaterialInstance instance(material);
    const auto revision = instance.GetRevision();

    EXPECT_FALSE(instance.SetScalar("Tint", 0.5f));
    EXPECT_TRUE(instance.SetVector("Tint", { 0.5f, 0.2f, 0.1f, 1.0f }));
    EXPECT_EQ(instance.GetRevision(), revision + 1);
}

TEST(MaterialTest, ValidatesTextureSampleAgainstTextureParameter)
{
    MaterialGraph graph;

    graph.SetChannel(
        EMaterialChannel::BaseColor,
        graph.AddNode<TextureSample>("AlbedoTexture")
    );

    auto material = CreateRef<Material>("Textured");
    material->SetGraph(std::move(graph));

    EXPECT_TRUE(material->DefineParameter("AlbedoTexture", {
        .Kind = EMaterialParameterKind::Texture,
        .DefaultValue = SMaterialParameter::MakeTexture(nullptr),
    }));
    EXPECT_TRUE(material->ValidateGraph());
}

TEST(MaterialTest, CreatesInstancesThatKeepTheirParentAlive)
{
    auto material = CreateRef<Material>("InstanceOwner");
    const auto instance = material->CreateInstance();
    const auto parent = instance->GetParent();
    material.reset();

    ASSERT_TRUE(instance);
    ASSERT_TRUE(parent);
    EXPECT_EQ(instance->GetParent(), parent);
}
