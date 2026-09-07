#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/MaterialProxyCache.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;
using namespace Elixir::Materials::Rendering;

namespace
{
    class CountingMaterialResolver final : public MaterialResolver
    {
    public:
        Ref<const MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& instance
        ) override
        {
            ++ResolveCount;

            const auto compiled = Compiler::Build(*instance->GetParent()).Material;
            return MaterialRenderProxy::Create(compiled, *instance);
        }

        uint32_t ResolveCount = 0;
    };

    Ref<MaterialInstance> CreateInstance()
    {
        const auto material = CreateRef<Material>("Cache test");
        EXPECT_TRUE(material->DefineParameter("Tint", {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialValueType::Float4,
            .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1.0f)),
        }));

        return material->CreateInstance();
    }
}

TEST(MaterialProxyCacheTest, ReusesTheProxyWhileTheInstanceAndMaterialAreCurrent)
{
    CountingMaterialResolver resolver;
    MaterialProxyCache cache(resolver);
    const auto instance = CreateInstance();

    const auto first = cache.Resolve(instance);
    const auto second = cache.Resolve(instance);

    ASSERT_TRUE(first);
    EXPECT_EQ(first, second);
    EXPECT_EQ(resolver.ResolveCount, 1);
}

TEST(MaterialProxyCacheTest, RebuildsTheProxyWhenTheInstanceRevisionChanges)
{
    CountingMaterialResolver resolver;
    MaterialProxyCache cache(resolver);
    const auto instance = CreateInstance();

    const auto first = cache.Resolve(instance);
    ASSERT_TRUE(instance->SetVector("Tint", { 0.2f, 0.4f, 0.6f, 1.0f }));
    const auto rebuilt = cache.Resolve(instance);

    ASSERT_TRUE(first);
    ASSERT_TRUE(rebuilt);
    EXPECT_NE(first, rebuilt);
    EXPECT_EQ(resolver.ResolveCount, 2);
}

TEST(MaterialProxyCacheTest, RebuildsTheProxyWhenTheParentMaterialRevisionChanges)
{
    CountingMaterialResolver resolver;
    MaterialProxyCache cache(resolver);
    const auto instance = CreateInstance();

    const auto first = cache.Resolve(instance);
    ASSERT_TRUE(instance->GetParent()->SetUsage(EMaterialUsage::ParticleSprite, true));
    const auto rebuilt = cache.Resolve(instance);

    ASSERT_TRUE(first);
    ASSERT_TRUE(rebuilt);
    EXPECT_NE(first, rebuilt);
    EXPECT_EQ(resolver.ResolveCount, 2);
}
