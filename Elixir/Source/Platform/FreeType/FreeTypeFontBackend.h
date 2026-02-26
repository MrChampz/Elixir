#pragma once

#include <Engine/Font/FontBackend.h>

#include <freetype/freetype.h>

namespace Elixir
{
    class ELIXIR_API FreeTypeFontBackend final : public FontBackend
    {
      public:
        static constexpr float PX_RANGE = 4.0;

        explicit FreeTypeFontBackend(const GraphicsContext* context);
        ~FreeTypeFontBackend() override;

        Ref<Font> Load(const std::filesystem::path& filepath) override;

      private:
        FT_Library m_Library;
        std::unordered_map<std::string, FT_Face> m_Fonts;

        const GraphicsContext* m_GraphicsContext;
    };
}