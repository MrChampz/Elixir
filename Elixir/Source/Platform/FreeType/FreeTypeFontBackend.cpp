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

    Ref<Font> FreeTypeFontBackend::Load(const std::filesystem::path& filepath)
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


            SFontCreateInfo info = {};
            info.Name = name;

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
                glyph.edgeColoring(&msdfgen::edgeColoringByDistance, maxCornerAngle, 0);
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

            GeneratorAttributes genAttribs;
            genAttribs.config.overlapSupport = true;
            genAttribs.scanlinePass = true;

            ImmediateAtlasGenerator<float, 4, mtsdfGenerator, BitmapAtlasStorage<uint8_t, 4>> mtsdfGenerator(width, height);
            mtsdfGenerator.setAttributes(genAttribs);
            mtsdfGenerator.setThreadCount(8);

            mtsdfGenerator.generate(glyphs.data(), glyphs.size());
            msdfgen::BitmapConstRef<uint8_t, 4> mtsdf = mtsdfGenerator.atlasStorage();
            const auto mtsdfInverted = InvertBitmap<4>(mtsdf);

            info.AscenderY = fontGeometry.getMetrics().ascenderY;
            info.DescenderY = fontGeometry.getMetrics().descenderY;
            info.Atlas.Info.PxRange = PX_RANGE;
            info.Atlas.Info.Width = width;
            info.Atlas.Info.Height = height;

            info.Atlas.MTSDF = Texture2D::Create(
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

                info.Glyphs.push_back(glyph);
            }

            Ref<Font> font = CreateRef<Font>(info);

            auto kerning = fontGeometry.getKerning();
            for (const auto& [codepoints, adjustment] : kerning)
            {
                const auto a = codepoints.first;
                const auto b = codepoints.second;
                font->SetKerning(a, b, (float)adjustment);
            }

            msdfgen::destroyFont(f);

            m_Fonts[name] = face;
            return std::move(font);
        }

        EE_CORE_FATAL("Cannot load font! [Path={0}]", filepath.string())
        return nullptr;
    }
}