#include <gtest/gtest.h>

#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/Nodes/Add.h>
#include <Engine/Materials/Nodes/Parameter.h>
#include <Engine/Materials/Nodes/TextureSample.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;

TEST(MaterialTest, StoresSurfaceShadingModel)
{
    Material material("ClearCoated");

    EXPECT_EQ(material.GetShadingModel(), EMaterialShadingModel::Lit);

    const auto revision = material.GetRevision();
    EXPECT_TRUE(material.SetShadingModel(EMaterialShadingModel::ClearCoat));
    EXPECT_EQ(material.GetShadingModel(), EMaterialShadingModel::ClearCoat);
    EXPECT_EQ(material.GetRevision(), revision + 1);
    EXPECT_FALSE(material.SetShadingModel(EMaterialShadingModel::ClearCoat));
}

TEST(MaterialTest, StoresTransparencySettings)
{
    Material material("Transparency");

    EXPECT_EQ(material.GetBlendMode(), EMaterialBlendMode::Opaque);
    EXPECT_FLOAT_EQ(material.GetAlphaCutoff(), 0.5f);

    const auto revision = material.GetRevision();
    EXPECT_TRUE(material.SetBlendMode(EMaterialBlendMode::Masked));
    EXPECT_EQ(material.GetBlendMode(), EMaterialBlendMode::Masked);
    EXPECT_EQ(material.GetRevision(), revision + 1);

    EXPECT_TRUE(material.SetAlphaCutoff(0.35f));
    EXPECT_FLOAT_EQ(material.GetAlphaCutoff(), 0.35f);
    EXPECT_EQ(material.GetRevision(), revision + 2);
}

TEST(MaterialTest, StoresDoubleSidedSetting)
{
    Material material("DoubleSided");

    EXPECT_FALSE(material.IsDoubleSided());
    
    const auto revision = material.GetRevision();
    EXPECT_TRUE(material.SetDoubleSided(true));
    EXPECT_TRUE(material.IsDoubleSided());
    EXPECT_EQ(material.GetRevision(), revision + 1);
    EXPECT_FALSE(material.SetDoubleSided(true));
}

TEST(MaterialTest, ClampsAlphaCutoff)
{
    Material material("Transparency");

    EXPECT_TRUE(material.SetAlphaCutoff(-0.01f));
    EXPECT_FLOAT_EQ(material.GetAlphaCutoff(), 0.0f);

    EXPECT_TRUE(material.SetAlphaCutoff(1.01f));
    EXPECT_FLOAT_EQ(material.GetAlphaCutoff(), 1.0f);

    EXPECT_FALSE(material.SetAlphaCutoff(1.5f));
}

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

TEST(MaterialTest, RejectsCyclicGraphConnections)
{
    MaterialGraph graph;
    const auto add = graph.AddNode<Add>();
    graph.Connect(add, add, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, add);

    Material material("Cyclic");
    material.SetGraph(std::move(graph));

    std::string error;
    EXPECT_FALSE(material.ValidateGraph(&error));
    EXPECT_NE(error.find("cycle"), std::string::npos);
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
