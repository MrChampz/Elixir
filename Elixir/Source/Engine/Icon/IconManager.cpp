#include "epch.h"
#include "IconManager.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Platform/LunaSVG/LunaSVGIconLoader.h>

namespace Elixir
{
    namespace
    {
        const GraphicsContext* s_GraphicsContext = nullptr;
        std::unordered_map<EIconFormat, Scope<IconLoader>> s_Loaders;

        uint64_t MakeRasterKey(const glm::uvec2 size)
        {
            return (static_cast<uint64_t>(size.x) << 32) | size.y;
        }
    }

    Icon::Icon(
        const GraphicsContext* context,
        SIconSource source,
        Ref<IconContent> content
    ) : m_GraphicsContext(context),
        m_Source(std::move(source)),
        m_Content(std::move(content)) {}

    Ref<Texture2D> Icon::GetTexture(const glm::vec2& logicalSize) const
    {
        EE_CORE_ASSERT(m_GraphicsContext, "Icon requires an initialized IconManager")
        if (!m_GraphicsContext) return nullptr;

        const float dpiScale = m_GraphicsContext->GetDPIScale();
        const glm::uvec2 pixelSize = glm::max(
            glm::uvec2(glm::round(glm::max(logicalSize, glm::vec2(1.0f)) * dpiScale)),
            glm::uvec2(1)
        );
        const uint64_t key = MakeRasterKey(pixelSize);
        if (const auto it = m_Textures.find(key); it != m_Textures.end())
            return it->second;

        const SIconBitmap bitmap = m_Content->Rasterize({ .PixelSize = pixelSize });
        if (!bitmap.IsValid())
        {
            EE_CORE_ERROR("Cannot rasterize icon! [Path={0}]", m_Source.Path.string())
            return nullptr;
        }

        const auto texture = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_UNORM,
            bitmap.Size.x,
            bitmap.Size.y,
            bitmap.Pixels.data(),
            m_Source.Path.string()
        );
        m_Textures.emplace(key, texture);
        return texture;
    }

    void IconManager::Initialize(const GraphicsContext* context)
    {
        EE_CORE_ASSERT(context, "IconManager requires a graphics context")
        if (s_GraphicsContext == context) return;

        s_GraphicsContext = context;
        s_Loaders.clear();
        RegisterLoader(CreateScope<LunaSVGIconLoader>());
    }

    void IconManager::Shutdown()
    {
        s_Loaders.clear();
        s_GraphicsContext = nullptr;
    }

    bool IconManager::RegisterLoader(Scope<IconLoader> loader)
    {
        if (!loader) return false;

        const EIconFormat format = loader->GetFormat();
        if (s_Loaders.contains(format)) return false;

        s_Loaders.emplace(format, std::move(loader));
        return true;
    }

    bool IconManager::ReplaceLoader(Scope<IconLoader> loader)
    {
        if (!loader) return false;

        s_Loaders[loader->GetFormat()] = std::move(loader);
        return true;
    }

    Ref<Icon> IconManager::Load(const std::filesystem::path& path)
    {
        EE_CORE_ASSERT(s_GraphicsContext, "IconManager must be initialized before loading icons")

        const auto format = InferFormat(path);
        if (!format)
        {
            EE_CORE_ERROR("Unsupported icon format! [Path={0}]", path.string())
            return nullptr;
        }

        const auto it = s_Loaders.find(*format);
        if (it == s_Loaders.end())
        {
            EE_CORE_ERROR("No loader registered for icon format! [Path={0}]", path.string())
            return nullptr;
        }

        SIconSource source{ .Path = path, .Format = *format };
        const auto content = it->second->Load(source);
        if (!content) return nullptr;

        return Ref<Icon>(new Icon(s_GraphicsContext, std::move(source), content));
    }

    std::optional<EIconFormat> IconManager::InferFormat(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });

        if (extension == ".svg") return EIconFormat::SVG;
        if (extension == ".png") return EIconFormat::PNG;
        if (extension == ".webp") return EIconFormat::WebP;
        return std::nullopt;
    }
}
