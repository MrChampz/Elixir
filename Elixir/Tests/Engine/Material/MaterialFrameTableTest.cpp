#include <gtest/gtest.h>

#include <Engine/Graphics/Texture.h>
#include <Engine/Material/MaterialFrameTable.h>
#include <Engine/Material/MaterialCompiler.h>

using namespace Elixir;

namespace
{
    class TestTexture final : public Texture
    {
    public:
        TestTexture()
            : Texture(nullptr, EImageFormat::R8G8B8A8_UNORM, 1) {}

        void Destroy() override {}
        void Resize(const Ref<CommandBuffer>& cmd, Extent3D extent) override {}
        void Transition(const CommandBuffer* cmd, EImageLayout layout) override {}

        void Copy(
            const CommandBuffer* cmd,
            Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        ) override {}

        void CopyFrom(
            const CommandBuffer* cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions
        ) override {}

        bool IsValid() const override { return true; }

    protected:
        void UpdateSampler() override {}
    };
}

TEST(MaterialFrameTableTest, DeduplicatesAProxyAndPreserveItsValues)
{
    auto material = CreateRef<Material>("Particle material");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleSprite, true));
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialGraphValueType::Float4,
        .DefaultValue = SMaterialParam::MakeVector({ 1.0f, 1.0f, 1.0f, 1.0f }),
    }));

    auto instance = CreateRef<MaterialInstance>(material);
    ASSERT_TRUE(instance->SetVector("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));

    const auto compiled = MaterialCompiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = MaterialRenderProxy::Create(compiled.Material, *instance);
    ASSERT_TRUE(proxy);

    MaterialFrameTable table(
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

TEST(MaterialFrameTableTest, RejectsAUniqueProxyPastCapacity)
{
    MaterialFrameTable table(
        0,
        0,
        [](const Ref<Texture>&) { return 0; }
    );

    auto material = CreateRef<Material>("Particle material");
    auto instance = CreateRef<MaterialInstance>(material);
    const auto compiled = MaterialCompiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = MaterialRenderProxy::Create(compiled.Material, *instance);
    ASSERT_TRUE(proxy);
    EXPECT_FALSE(table.Add(*proxy));
}

TEST(MaterialFrameTableTest, ResolvesAuthoredTextureSlots)
{
    const auto texture = CreateRef<TestTexture>();

    auto material = CreateRef<Material>("Particle material");
    ASSERT_TRUE(material->DefineParameter("Albedo", {
        .Kind = EMaterialParameterKind::Texture,
        .DefaultValue = SMaterialParam::MakeTexture(texture),
    }));

    auto instance = CreateRef<MaterialInstance>(material);
    const auto compiled = MaterialCompiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = MaterialRenderProxy::Create(compiled.Material, *instance);
    ASSERT_TRUE(proxy);

    uint32_t resolveCount = 0;
    MaterialFrameTable table(
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