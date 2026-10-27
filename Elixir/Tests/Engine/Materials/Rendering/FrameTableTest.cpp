#include <gtest/gtest.h>

#include <Engine/Graphics/Texture.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/Rendering/FrameTable.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;
using namespace Elixir::Materials::Rendering;

namespace
{
    class TestTexture final : public Texture
    {
    public:
        TestTexture()
            : Texture(nullptr, {}) {}
    };
}

TEST(FrameTableTest, DeduplicatesAProxyAndPreservesItsValues)
{
    auto material = CreateRef<Material>("Particle material", EMaterialUsage::Particle);
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector({ 1.0f, 1.0f, 1.0f, 1.0f }),
    }));

    auto instance = CreateRef<MaterialInstance>(material);
    ASSERT_TRUE(instance->SetVector("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));

    const auto compiled = Compiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = MaterialRenderProxy::Create(compiled.Material, *instance);
    ASSERT_TRUE(proxy);

    FrameTable table(
        1,
        17,
        [](const Ref<Texture>&) { return 23; }
    );
    const auto first = table.Add(*proxy);
    const auto second = table.Add(*proxy);

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(*first, 0);
    EXPECT_EQ(*second, 0);
    ASSERT_EQ(table.GetCount(), 1);

    const auto& data = table.GetData()[0];
    EXPECT_EQ(data.Values[0], glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
    EXPECT_EQ(data.TextureIndices[0], 17);
    EXPECT_EQ(data.TextureIndices.back(), 17);
}

TEST(FrameTableTest, RejectsAUniqueProxyPastCapacity)
{
    FrameTable table(
        0,
        0,
        [](const Ref<Texture>&) { return 0; }
    );

    auto material = CreateRef<Material>("Particle material");
    auto instance = CreateRef<MaterialInstance>(material);
    const auto compiled = Compiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = MaterialRenderProxy::Create(compiled.Material, *instance);
    ASSERT_TRUE(proxy);
    EXPECT_FALSE(table.Add(*proxy));
}

TEST(FrameTableTest, ResolvesAuthoredTextureSlots)
{
    const auto texture = CreateRef<TestTexture>();

    auto material = CreateRef<Material>("Particle material");
    ASSERT_TRUE(material->DefineParameter("Albedo", {
        .Kind = EMaterialParameterKind::Texture,
        .DefaultValue = SMaterialParameter::MakeTexture(texture),
    }));

    auto instance = CreateRef<MaterialInstance>(material);
    const auto compiled = Compiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = MaterialRenderProxy::Create(compiled.Material, *instance);
    ASSERT_TRUE(proxy);

    uint32_t resolveCount = 0;
    FrameTable table(
        1,
        5,
        [&resolveCount, &texture](const Ref<Texture>& resolved)
        {
            ++resolveCount;
            EXPECT_EQ(resolved, texture);
            return 37;
        }
    );

    ASSERT_TRUE(table.Add(*proxy));
    ASSERT_EQ(table.GetCount(), 1);

    const auto& data = table.GetData()[0];
    EXPECT_EQ(resolveCount, 1);
    EXPECT_EQ(data.TextureIndices[0], 37);
    EXPECT_EQ(data.TextureIndices[1], 5);
}

TEST(FrameTableTest, TextureResolutionFailureDoesNotPublishOrConsumeCapacityAndCanBeRetried)
{
    auto material = CreateRef<Material>("Failed texture registration");
    ASSERT_TRUE(material->DefineParameter("Albedo", {
        .Kind = EMaterialParameterKind::Texture,
        .DefaultValue = SMaterialParameter::MakeTexture(CreateRef<TestTexture>()),
    }));
    const auto instance = CreateRef<MaterialInstance>(material);
    const auto compiled = Compiler::Build(*material);
    ASSERT_TRUE(compiled);
    const auto proxy = MaterialRenderProxy::Create(compiled.Material, *instance);
    ASSERT_TRUE(proxy);

    bool available = false;
    uint32_t attempts = 0;
    FrameTable table(2, 5, [&](const Ref<Texture>&) -> std::optional<uint32_t>
    {
        ++attempts;
        return available ? std::optional<uint32_t>(37) : std::nullopt;
    });
    const auto validMaterial = CreateRef<Material>("Valid material");
    const auto validInstance = CreateRef<MaterialInstance>(validMaterial);
    const auto validCompiled = Compiler::Build(*validMaterial);
    ASSERT_TRUE(validCompiled);
    const auto validProxy = MaterialRenderProxy::Create(validCompiled.Material, *validInstance);
    ASSERT_TRUE(validProxy);
    ASSERT_TRUE(table.Add(*validProxy));

    EXPECT_FALSE(table.Add(*proxy));
    EXPECT_FALSE(table.Find(*proxy));
    ASSERT_EQ(table.GetCount(), 1u);
    ASSERT_TRUE(table.Find(*validProxy));
    EXPECT_EQ(*table.Find(*validProxy), 0u);

    available = true;
    const auto index = table.Add(*proxy);
    ASSERT_TRUE(index);
    EXPECT_EQ(*index, 1u);
    ASSERT_EQ(table.GetCount(), 2u);
    EXPECT_EQ(table.GetData()[1].TextureIndices[0], 37u);
    EXPECT_EQ(table.GetData()[1].TextureIndices[1], 5u);
    EXPECT_TRUE(table.Add(*proxy));
    EXPECT_EQ(attempts, 2u);
}
