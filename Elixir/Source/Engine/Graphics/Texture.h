#pragma once

#include <Engine/Graphics/Image.h>

namespace Elixir
{
    class ELIXIR_API Texture : public Image
    {
      public:
        ~Texture() override = default;

        [[nodiscard]] const std::string& GetPath() const { return m_Path; }

        static Ref<Texture> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr,
            const std::string& path = ""
        );

      protected:
        Texture(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr,
            const std::string& path = ""
        );
        Texture(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );

        std::string m_Path;
    };

    class ELIXIR_API Texture2D : public Texture
    {
      public:
        ~Texture2D() override = default;

        [[nodiscard]] uint32_t GetHeight() const { return m_Extent.Height; }

        static Ref<Texture2D> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            void* data = nullptr,
            const std::string& path = ""
        );

        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            void* data = nullptr
        );

      protected:
        Texture2D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            void* data = nullptr,
            const std::string& path = ""
        );
        Texture2D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
    };

    class ELIXIR_API Texture3D : public Texture2D
    {
    public:
        ~Texture3D() override = default;

        [[nodiscard]] uint32_t GetDepth() const { return m_Extent.Depth; }

        static Ref<Texture3D> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            void* data = nullptr,
            const std::string& path = ""
        );

        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            void* data = nullptr
        );

    protected:
        Texture3D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            void* data = nullptr,
            const std::string& path = ""
        );
        Texture3D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
    };
}