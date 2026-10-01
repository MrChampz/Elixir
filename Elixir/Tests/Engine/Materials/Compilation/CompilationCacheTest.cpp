#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/CompilationCache.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;

TEST(CompilationCacheTest, ReusesACompiledMaterialUntilTheSourceRevisionChanges)
{
    CompilationCache cache{ nullptr };
    const auto material = CreateRef<Material>("Cache test", EMaterialUsage::Particle);

    const auto first = cache.GetOrCompile(material);
    const auto second = cache.GetOrCompile(material);

    ASSERT_TRUE(first);
    EXPECT_EQ(first, second);

    ASSERT_TRUE(material->SetBlendMode(EMaterialBlendMode::Translucent));

    const auto rebuilt = cache.GetOrCompile(material);
    ASSERT_TRUE(rebuilt);

    EXPECT_NE(first, rebuilt);
    EXPECT_EQ(rebuilt->GetUsage(), EMaterialUsage::Particle);
}
