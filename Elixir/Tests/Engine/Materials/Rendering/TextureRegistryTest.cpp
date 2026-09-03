#include <gtest/gtest.h>

#include <Engine/Materials/Rendering/TextureRegistry.h>

using namespace Elixir;
using namespace Elixir::Materials::Rendering;

TEST(TextureRegistryTest, UsesFallbackUntilDescriptorIsVisible)
{
    constexpr uint32_t fallbackIndex = 3;

    const STextureBinding binding{
        .Handle = SResourceHandle::Texture(17),
        .ReadySubmission = 8,
    };

    EXPECT_EQ(binding.GetIndexForSubmission(7, fallbackIndex), fallbackIndex);
    EXPECT_EQ(binding.GetIndexForSubmission(8, fallbackIndex), 17);
    EXPECT_EQ(binding.GetIndexForSubmission(9, fallbackIndex), 17);
}
