#include "epch.h"
#include "IconManager.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Platform/LunaSVG/LunaSVGIconLoader.h>

namespace Elixir
{
    const GraphicsContext* s_GraphicsContext = nullptr;
    std::unordered_map<EIconFormat, Scope<IconLoader>> s_Loaders;

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
