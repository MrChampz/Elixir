#include <gtest/gtest.h>

#include <Engine/Material/MaterialRenderProxy.h>

using namespace Elixir;

TEST(MaterialRenderProxyTest, ResolvesOverridesIntoAnImmutableSnapshot)
{
    const auto material = CreateRef<Material>("Tinted");
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1.0f)),
    }));

    const auto compiled = MaterialCompiler::Build(*material).Material;
    ASSERT_TRUE(compiled);

    MaterialInstance instance(material);
    ASSERT_TRUE(instance.SetVector("Tint", { 0.2f, 0.4f, 0.6, 1.0f }));

    const auto proxy = MaterialRenderProxy::Create(compiled, instance);
    ASSERT_TRUE(proxy);
    ASSERT_EQ(proxy->GetValues().size(), 1);
    EXPECT_EQ(proxy->GetValues()[0], glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
    EXPECT_EQ(proxy->GetInstanceRevision(), instance.GetRevision());
}

TEST(MaterialRenderProxyTest, RejectsACompiledMaterialForAnOldSchema)
{
    const auto material = CreateRef<Material>("Tinted");
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1.0f)),
    }));

    const auto compiled = MaterialCompiler::Build(*material).Material;
    ASSERT_TRUE(compiled);
    ASSERT_TRUE(material->SetDefaultParameter(
        "Tint",
        SMaterialParameter::MakeVector(glm::vec4(0.5f))
    ));

    MaterialInstance instance(material);
    EXPECT_FALSE(MaterialRenderProxy::Create(compiled, instance));
}
