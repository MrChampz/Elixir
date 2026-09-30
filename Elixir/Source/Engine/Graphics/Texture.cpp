#include "epch.h"
#include "Texture.h"

namespace Elixir
{
    Texture::Texture(Ref<Image> image, const STextureCreateInfo& info)
        : m_Image(std::move(image)), m_Path(info.Path), m_HDR(info.HDR)
    {
    }

    Ref<Texture> Texture::Create(const GraphicsContext* context, const STextureCreateInfo& info)
    {
        const auto image = Image::Create(context, CreateImageInfo(info));
        if (!image) return nullptr;

        return Ref<Texture>(new Texture(image, info));
    }

    Ref<Texture> Texture::Create(
        const GraphicsContext* context, const EImageFormat format, const uint32_t width,
        const void* data, const std::string& path
    )
    {
        STextureCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Path = path;
        return Create(context, info);
    }

    SImageCreateInfo Texture::CreateImageInfo(const STextureCreateInfo& info)
    {
        return {
            .InitialData = info.InitialData,
            .Width = info.Width,
            .Type = EImageType::_1D,
            .Format = info.Format,
            .MipmapMode = info.MipmapMode,
            .MipLevels = info.MipLevels,
            .Usage = EImageUsage::Sampled,
            .InitialLayout = EImageLayout::ShaderReadOnly,
            .InitialMipData = info.InitialMipData,
        };
    }

    SImageCreateInfo Texture::CreateImageInfo(
        const EImageFormat format, const uint32_t width,
        const void* data, const std::string& path
    )
    {
        STextureCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Path = path;
        return CreateImageInfo(info);
    }

    Texture2D::Texture2D(Ref<Image> image, const STextureCreateInfo& info)
        : Texture(std::move(image), info)
    {
    }

    Ref<Texture2D> Texture2D::Create(const GraphicsContext* context, const STexture2DCreateInfo& info)
    {
        const auto image = Image::Create(context, CreateImageInfo(info));
        if (!image) return nullptr;

        return Ref<Texture2D>(new Texture2D(image, info));
    }

    Ref<Texture2D> Texture2D::Create(
        const GraphicsContext* context, const EImageFormat format,
        const uint32_t width, const uint32_t height, const void* data, const std::string& path
    )
    {
        STexture2DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        info.Path = path;
        return Create(context, info);
    }

    SImageCreateInfo Texture2D::CreateImageInfo(const STexture2DCreateInfo& info)
    {
        auto image = Texture::CreateImageInfo(info);
        image.Type = EImageType::_2D;
        image.Height = info.Height;
        return image;
    }

    SImageCreateInfo Texture2D::CreateImageInfo(
        const EImageFormat format, const uint32_t width, const uint32_t height, const void* data
    )
    {
        STexture2DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        return CreateImageInfo(info);
    }

    Texture3D::Texture3D(Ref<Image> image, const STextureCreateInfo& info)
        : Texture(std::move(image), info)
    {
    }

    Ref<Texture3D> Texture3D::Create(const GraphicsContext* context, const STexture3DCreateInfo& info)
    {
        const auto image = Image::Create(context, CreateImageInfo(info));
        if (!image) return nullptr;

        return Ref<Texture3D>(new Texture3D(image, info));
    }

    Ref<Texture3D> Texture3D::Create(
        const GraphicsContext* context, const EImageFormat format,
        const uint32_t width, const uint32_t height, const uint32_t depth,
        const void* data, const std::string& path
    )
    {
        STexture3DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        info.Depth = depth;
        info.Path = path;
        return Create(context, info);
    }

    SImageCreateInfo Texture3D::CreateImageInfo(const STexture3DCreateInfo& info)
    {
        auto image = Texture2D::CreateImageInfo(info);
        image.Type = EImageType::_3D;
        image.Depth = info.Depth;
        return image;
    }

    SImageCreateInfo Texture3D::CreateImageInfo(
        const EImageFormat format, const uint32_t width, const uint32_t height,
        const uint32_t depth, const void* data
    )
    {
        STexture3DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        info.Depth = depth;
        return CreateImageInfo(info);
    }
}
