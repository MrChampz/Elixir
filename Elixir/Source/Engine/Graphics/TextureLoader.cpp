#include "epch.h"
#include "TextureLoader.h"

#include "Utils.h"

#include <stb_image.h>

namespace Elixir
{
    using namespace Elixir::Graphics;

    void TraceTextureInfo(
        const std::string& path,
        bool isHdr,
        const EImageFormat format,
        int channelCount
    )
    {
        EE_CORE_TRACE(
            "Creating texture {0} [HDR = {1}, Format = {2}, Channels = {3}].", path, isHdr,
            format, channelCount
        )
    }

    const GraphicsContext* TextureLoader::s_Context = nullptr;
    bool TextureLoader::s_Initialized = false;

    void TextureLoader::Initialize(const GraphicsContext* context)
    {
        s_Context = context;
        s_Initialized = true;
    }

    Ref<Texture> TextureLoader::Load(const std::filesystem::path& path)
    {
        EE_CORE_ASSERT(s_Initialized, "TextureLoader is not initialized!")

        void* data;
        bool isHdr = false;
        int width, height, channels;
        EImageFormat format;

        const auto pathStr = path.string();

        EE_CORE_TRACE("Loading texture data: {0}.", pathStr)

        if (stbi_is_hdr(pathStr.c_str()))
        {
            isHdr = true;
            data = stbi_loadf(
                pathStr.c_str(),
                &width,
                &height,
                &channels,
                STBI_rgb_alpha
            );
            format = EImageFormat::R16G16B16A16_SFLOAT;
        }
        else
        {
            data = stbi_load(
                pathStr.c_str(),
                &width,
                &height,
                &channels,
                STBI_rgb_alpha
            );
            format = EImageFormat::R8G8B8A8_SRGB;
        }

        EE_CORE_ASSERT(data, "Could not read texture data!")

        TraceTextureInfo(pathStr, isHdr, format, channels);

        auto texture = Texture2D::Create(
            s_Context,
            format,
            width,
            height,
            data,
            pathStr
        );
        texture->m_HDR = isHdr;
        EE_CORE_TRACE("Loaded texture: {0} [{1}].", pathStr, texture->GetUUID())

        stbi_image_free(data);

        return std::move(texture);
    }
}