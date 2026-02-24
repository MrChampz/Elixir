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

        Ref<SFont> Load(const std::filesystem::path& filepath) override;

        glm::vec2 MeasureText(
            const std::string& text,
            const Ref<SFont>& font,
            float fontSize
        ) override;

        float GetLineHeight(const Ref<SFont>& font, float fontSize) override;

      private:
        FT_Library m_Library;
        std::unordered_map<std::string, FT_Face> m_Fonts;

        const GraphicsContext* m_GraphicsContext;
    };
}