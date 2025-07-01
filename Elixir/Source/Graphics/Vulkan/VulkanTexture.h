#pragma once

#include <Engine/Graphics/Texture.h>
#include <Graphics/Vulkan/VulkanImage.h>

namespace Elixir::Vulkan
{
    class ELIXIR_API VulkanTexture final : public VulkanBaseImage<Texture>
    {
      public:
        VulkanTexture(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr,
            const std::string& path = ""
        );
        ~VulkanTexture() override;

        using Image::Transition;
        using Image::Copy;
        using Image::CopyFrom;

      protected:
        VulkanTexture(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
    };

    class ELIXIR_API VulkanTexture2D final : public VulkanBaseImage<Texture2D>
    {
      public:
        VulkanTexture2D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            void* data = nullptr,
            const std::string& path = ""
        );
        ~VulkanTexture2D() override;

      protected:
        VulkanTexture2D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
    };

    class ELIXIR_API VulkanTexture3D final : public VulkanBaseImage<Texture3D>
    {
      public:
        VulkanTexture3D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            void* data = nullptr,
            const std::string& path = ""
        );
        ~VulkanTexture3D() override;

      protected:
        VulkanTexture3D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
    };
}