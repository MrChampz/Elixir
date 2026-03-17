#include "epch.h"
#include "FontManager.h"

#include <Platform/FreeType/FreeTypeFontBackend.h>

namespace Elixir
{
    bool FontManager::s_Initialized = false;
    Scope<FontBackend> FontManager::s_FontBackend = nullptr;
    Ref<TextureSet> FontManager::s_FontsAtlases = nullptr;
    std::unordered_map<std::string, Ref<Font>> FontManager::s_Fonts;
    const GraphicsContext* FontManager::s_GraphicsContext = nullptr;

    void FontManager::Initialize(const GraphicsContext* context)
    {
        EE_PROFILE_ZONE_SCOPED()

        if (!s_Initialized)
        {
            s_GraphicsContext = context;
            s_FontBackend = CreateScope<FreeTypeFontBackend>(s_GraphicsContext);
            s_FontsAtlases = TextureSet::Create(context);
            s_Initialized = true;
            EE_CORE_INFO("Font Manager initialized.")

            // Load the default font
            Load("./Assets/Fonts/" + DEFAULT_FONT_NAME + ".otf");
        }
    }

    void FontManager::Shutdown()
    {
        EE_PROFILE_ZONE_SCOPED()
        s_Fonts.clear();
        s_FontsAtlases.reset();
        s_FontBackend.reset();
        s_Initialized = false;
        EE_CORE_INFO("Font Manager shutdown.")
    }

    Ref<Font> FontManager::GetDefaultFont()
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(!s_Fonts.empty(), "Default font should be loaded before calling GetDefaultFont()!")

        const auto it = s_Fonts.find(DEFAULT_FONT_NAME);
        if (it != s_Fonts.end())
            return it->second;

        return s_Fonts.begin()->second;
    }

    Ref<Font> FontManager::GetFont(const std::string& name)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto it = s_Fonts.find(name);
        if (it != s_Fonts.end())
            return it->second;

        EE_CORE_WARN("Font not found: {0}", name)
        return nullptr;
    }

    Ref<Font> FontManager::Load(const std::filesystem::path& filepath)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto name = filepath.stem().string();

        // Try to get from cache
        const auto it = s_Fonts.find(name);
        if (it != s_Fonts.end())
            return it->second;

        // If not found, call the backend to load it
        EE_CORE_TRACE("Loading font: {0}", filepath.string())
        const auto font = s_FontBackend->Load(filepath);

        // Bind the atlas to atlases texture set and save the returned index
        const auto handle = s_FontsAtlases->AddTexture(font->GetMTSDF());
        font->SetAtlasHandle(handle);

        EE_CORE_TRACE("Loaded font: {0}", filepath.string())
        s_Fonts[name] = std::move(font);
        return s_Fonts[name];
    }

    glm::vec2 FontManager::MeasureText(
        const std::string& text,
        const Ref<Font>& font,
        const float fontSize
    )
    {
        EE_PROFILE_ZONE_SCOPED()
        return font->MeasureText(text, fontSize);
    }

    float FontManager::GetLineHeight(const Ref<Font>& font, const float fontSize)
    {
        EE_PROFILE_ZONE_SCOPED()
        return font->GetLineHeight(fontSize);
    }
}