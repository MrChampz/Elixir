#include "epch.h"
#include "Font.h"

namespace Elixir
{
    Font::Font(const SFontCreateInfo& info)
        : m_Name(info.Name),
          m_Atlas(info.Atlas),
          m_AscenderY(info.AscenderY),
          m_DescenderY(info.DescenderY)
    {
        EE_PROFILE_ZONE_SCOPED()
        InitGlyphs(info);
    }

    glm::vec2 Font::MeasureText(const std::string& text, const float fontSize) const
    {
        const float scale = 1.0f / (m_AscenderY - m_DescenderY);
        float width = 0.0f;

        int i = 0;
        while (i < (int)text.size())
        {
            const auto charLen = UTF8::UTF8CharLength(text[i]);
            const auto codepoint = UTF8::UTF8ToCodepoint(text, i);

            const auto glyph = GetGlyph(codepoint);

            if (glyph.has_value())
            {
                width += glyph->Advance * scale * fontSize;
            }

            i += charLen;
        }

        float height = GetLineHeight(fontSize);

        return { width, height };
    }

    float Font::GetScale() const
    {
        return 1.0f / (m_AscenderY - m_DescenderY);
    }

    float Font::GetLineHeight(const float fontSize) const
    {
        return (m_AscenderY - m_DescenderY) * GetScale() * fontSize;
    }

    std::optional<const SGlyph> Font::GetGlyph(const int codepoint) const
    {
        if (m_Glyphs.contains(codepoint))
            return std::optional(m_Glyphs.at(codepoint));

        return std::nullopt;
    }

    float Font::GetKerning(const int a, const int b)
    {
        const auto key = ((uint64_t)a << 32) | (uint32_t)b;
        return m_Kerning[key];
    }

    void Font::SetKerning(const int a, const int b, const float kerning)
    {
        const auto key = ((uint64_t)a << 32) | (uint32_t)b;
        m_Kerning[key] = kerning;
    }

    void Font::InitGlyphs(const SFontCreateInfo& info)
    {
        m_Glyphs.reserve(info.Glyphs.size());
        for (const auto& glyph : info.Glyphs)
        {
            m_Glyphs[glyph.Unicode] = glyph;
        }
    }
}