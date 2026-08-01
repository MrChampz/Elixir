#include <gtest/gtest.h>

#include <Engine/Material/MaterialTextureRegistry.h>

using namespace Elixir;

TEST(MaterialTextureRegistryTest, UsesFallbackUntilDescriptorIsVisible)
{
    constexpr uint32_t fallbackIndex = 3;

    const SMaterialTextureBinding binding{
        .   Handle = SResourceHandle::Texture(17),
        .ReadySubmission = 8,
    };

    EXPECT_EQ(binding.GetIndexForSubmission(7, fallbackIndex), fallbackIndex);
    EXPECT_EQ(binding.GetIndexForSubmission(8, fallbackIndex), 17);
    EXPECT_EQ(binding.GetIndexForSubmission(9, fallbackIndex), 17);
}