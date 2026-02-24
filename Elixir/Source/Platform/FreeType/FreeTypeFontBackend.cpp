#include "epch.h"
#include "FreeTypeFontBackend.h"

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdfgen/msdfgen.h>
#include <msdfgen/msdfgen-ext.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "spdlog/fmt/bundled/base.h"
using namespace msdf_atlas;

namespace Elixir
{
    template <int N>
    std::vector<uint8_t> InvertBitmap(const msdfgen::BitmapConstRef<uint8_t, N>& bitmap)
    {
        std::vector<uint8_t> inverted;

        for (int y = 0; y < bitmap.height; ++y)
        {
            const auto flippedY = bitmap.height - y - 1;
            for (int x = 0; x < bitmap.width; ++x)
            {
                for (int c = 0; c < N; ++c)
                {
                    inverted.push_back(bitmap(x, flippedY)[c]);
                }
            }
        }

        return inverted;
    }

    FreeTypeFontBackend::FreeTypeFontBackend(const GraphicsContext* context)
        : m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(!FT_Init_FreeType(&m_Library), "Could not init FreeType!");
    }

    FreeTypeFontBackend::~FreeTypeFontBackend()
    {
        EE_PROFILE_ZONE_SCOPED()

        for (const auto& face : m_Fonts | std::views::values)
        {
            FT_Done_Face(face);
        }
        m_Fonts.clear();

        FT_Done_FreeType(m_Library);
    }

    Ref<SFont> FreeTypeFontBackend::Load(const std::filesystem::path& filepath)
    {
        EE_PROFILE_ZONE_SCOPED()

        FT_Face face;

        if (FT_New_Face(m_Library, filepath.string().c_str(), 0, &face))
        {
            EE_CORE_FATAL("Cannot load font! [Path={0}]", filepath.string())
            return nullptr;
        }

        if (auto f = msdfgen::adoptFreetypeFont(face))
        {
            const auto name = filepath.stem().string();

            Ref<SFont> font = CreateRef<SFont>();
            font->Name = name;

            std::vector<GlyphGeometry> glyphs;
            FontGeometry fontGeometry(&glyphs);

            struct SCharsetRange
            {
                uint32_t Begin, End;
            };

            static constexpr SCharsetRange charsetRanges[] =
            {
                { 0x0020, 0x00FF }
            };

            Charset charset;
            for (auto& [begin, end] : charsetRanges)
            {
                for (uint32_t c = begin; c <= end; c++)
                    charset.add(c);
            }

            const auto count = fontGeometry.loadCharset(f, 1.0, charset);
            EE_CORE_TRACE("Loaded {0} glyphs (out of {1}) [Font = {2}].", count, charset.size(), name)

            for (auto& glyph : glyphs)
            {
                constexpr double maxCornerAngle = 3.0;
                glyph.edgeColoring(&msdfgen::edgeColoringSimple, maxCornerAngle, 0);
            }

            TightAtlasPacker packer;
            packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
            packer.setMinimumScale(64.0);
            packer.setPixelRange(PX_RANGE);
            packer.setMiterLimit(1.0);

            const auto remaining = packer.pack(glyphs.data(), glyphs.size());
            EE_CORE_ASSERT(remaining == 0, "Failed to pack glyphs!")

            int width = 0, height = 0;
            packer.getDimensions(width, height);

            ImmediateAtlasGenerator<float, 4, mtsdfGenerator, BitmapAtlasStorage<uint8_t, 4>> mtsdfGenerator(width, height);
            mtsdfGenerator.setThreadCount(8);

            mtsdfGenerator.generate(glyphs.data(), glyphs.size());
            msdfgen::BitmapConstRef<uint8_t, 4> mtsdf = mtsdfGenerator.atlasStorage();
            const auto mtsdfInverted = InvertBitmap<4>(mtsdf);

            font->Atlas.Info.PxRange = PX_RANGE;
            font->Atlas.Info.AscenderY = fontGeometry.getMetrics().ascenderY;
            font->Atlas.Info.DescenderY = fontGeometry.getMetrics().descenderY;
            font->Atlas.Info.Width = width;
            font->Atlas.Info.Height = height;

            font->Atlas.MTSDF = Texture2D::Create(
                m_GraphicsContext,
                EImageFormat::R8G8B8A8_UNORM,
                width, height,
                mtsdfInverted.data()
            );

            for (auto& g : glyphs)
            {
                SGlyph glyph;
                glyph.Unicode = g.getCodepoint();
                glyph.Advance = g.getAdvance();

                double pl, pb, pr, pt;
                g.getQuadPlaneBounds(pl, pb, pr, pt);
                glyph.PlaneBounds = SRect{
                    { float(pl),  float(pb) },
                    { float(pr), float(pt) }
                };

                double al, ab, ar, at;
                g.getQuadAtlasBounds(al, ab, ar, at);
                glyph.AtlasBounds = SRect{
                    { float(al), float(ab) },
                    { float(ar), float(at) }
                };

                font->Glyphs.push_back(glyph);
            }

            msdfgen::destroyFont(f);

            m_Fonts[name] = face;
            return std::move(font);
        }

        EE_CORE_FATAL("Cannot load font! [Path={0}]", filepath.string())
        return nullptr;
    }

    glm::vec2 FreeTypeFontBackend::MeasureText(
        const std::string& text,
        const Ref<SFont>& font,
        const float fontSize
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto it = m_Fonts.find(font->Name);
        if (it == m_Fonts.end())
            return {};

        const FT_Face face = it->second;

        FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fontSize);

        float totalWidth = 0.0f;
        float maxHeight = 0.0f;
        float maxBearingY = 0.0f;
        float maxDescender = 0.0f;

        for (const auto c : text)
        {
            if (FT_Load_Char(face, c, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING))
                continue;

            const auto glyph = face->glyph;

            printf("Char: %c, FT advance: %ld\n", c, (glyph->advance.x >> 6));
            printf("Char: %c, MSDF advance: %f\n", c, font->GetGlyph(c)->Advance);

            totalWidth += glyph->advance.x >> 6;

            const float glyphHeight = glyph->bitmap.rows;
            const float bearingY = glyph->bitmap_top;
            const float descender = glyphHeight - bearingY;

            maxBearingY = std::max(maxBearingY, bearingY);
            maxDescender = std::max(maxDescender, descender);
        }

        maxHeight = maxBearingY + maxDescender;

        return { totalWidth, maxHeight };
    }

    float FreeTypeFontBackend::GetLineHeight(const Ref<SFont>& font, float fontSize)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto it = m_Fonts.find(font->Name);
        if (it == m_Fonts.end())
            return fontSize * 1.2f;

        const FT_Face face = it->second;
        FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fontSize);

        return (face->size->metrics.height >> 6);
    }
}