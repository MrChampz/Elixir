#include "epch.h"
#include "FontManager.h"

#include "Engine/Graphics/TextureLoader.h"

#include <simdjson.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdfgen/msdfgen.h>
#include <msdfgen/msdfgen-ext.h>
#include <ft2build.h>
#include FT_FREETYPE_H
using namespace msdf_atlas;                                                     

namespace Elixir::GUI
{
    bool FontManager::s_Initialized = false;
    const GraphicsContext* FontManager::s_GraphicsContext = nullptr;
    std::unordered_map<std::string, Ref<SFont>> FontManager::s_Fonts;

    bool FileExists(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
    }

    SAtlas LoadAtlas(simdjson::ondemand::document& fontData, const std::filesystem::path& atlasPath)
    {
        auto atlasData = fontData.get_object()["atlas"].get_object();
        auto metricsData = fontData.get_object()["metrics"].get_object();

        SAtlas atlas;
        atlas.Info.PxRange = float(atlasData["distanceRange"]);
        atlas.Info.Width = int(atlasData["width"]);
        atlas.Info.Height = int(atlasData["height"]);
        atlas.Info.AscenderY = 1.0820000000000001;
        atlas.Info.DescenderY = -0.251;

        atlas.MTSDFTexture = TextureLoader::Load(atlasPath, EImageFormat::R8G8B8A8_UNORM);

        return atlas;
    }

    SRect ParseRect(simdjson::ondemand::object rectData)
    {
        SRect rect;
        rect.Position.x = float(rectData["left"]);
        rect.Position.y = float(rectData["bottom"]); // bottom
        rect.Size.x     = float(rectData["right"]);
        rect.Size.y     = float(rectData["top"]); // top

        return rect;
    }

    SGlyph ParseGlyph(simdjson::ondemand::object glyphData)
    {
        SGlyph glyph;
        glyph.Unicode = int(glyphData["unicode"]);
        glyph.Advance = float(glyphData["advance"]);

        simdjson::ondemand::value planeBounds;
        if (glyphData["planeBounds"].get(planeBounds) == simdjson::SUCCESS)
        {
            glyph.PlaneBounds = ParseRect(planeBounds.get_object());
        }

        simdjson::ondemand::value atlasBounds;
        if (glyphData["atlasBounds"].get(atlasBounds) == simdjson::SUCCESS)
        {
            glyph.AtlasBounds = ParseRect(atlasBounds.get_object());
        }

        return glyph;
    }

    std::vector<SGlyph> LoadGlyphs(simdjson::ondemand::document& fontData)
    {
        std::vector<SGlyph> glyphs;
        for (auto glyph : fontData["glyphs"].get_array())
        {
            glyphs.push_back(ParseGlyph(glyph.get_object()));
        }

        return glyphs;
    }

    void FontManager::Initialize(const GraphicsContext* context)
    {
        EE_PROFILE_ZONE_SCOPED()

        if (!s_Initialized)
        {
            s_GraphicsContext = context;
            s_Initialized = true;
            EE_CORE_INFO("Font Manager initialized.")
        }
    }

    void FontManager::Shutdown()
    {
        EE_PROFILE_ZONE_SCOPED()
        s_Fonts.clear();
        EE_CORE_INFO("Font Manager shutdown.")
    }

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

    Ref<SFont> FontManager::Load(const std::filesystem::path& filepath)
    {
        EE_PROFILE_ZONE_SCOPED()

        if (msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype())
        {
            if (msdfgen::FontHandle* font = msdfgen::loadFont(ft, filepath.string().c_str()))
            {
                const auto name = filepath.stem().string();
                constexpr auto pxRange = 4.0;

                Ref<SFont> fontc = CreateRef<SFont>();
                fontc->Name = name;

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

                const auto count = fontGeometry.loadCharset(font, 1.0, charset);
                EE_CORE_TRACE("Loaded {0} glyphs (out of {1}) [Font = {2}].", count, charset.size(), name)

                for (auto& glyph : glyphs)
                {
                    constexpr double maxCornerAngle = 3.0;
                    glyph.edgeColoring(&msdfgen::edgeColoringSimple, maxCornerAngle, 0);
                }

                TightAtlasPacker packer;
                packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
                packer.setMinimumScale(64.0);
                packer.setPixelRange(pxRange);
                packer.setMiterLimit(1.0);

                const auto remaining = packer.pack(glyphs.data(), glyphs.size());
                EE_CORE_ASSERT(remaining == 0, "Failed to pack glyphs!")

                int width = 0, height = 0;
                packer.getDimensions(width, height);

                ImmediateAtlasGenerator<float, 1, scanlineGenerator, BitmapAtlasStorage<uint8_t, 1>> hardmaskGenerator(width, height);
                hardmaskGenerator.setThreadCount(8);

                hardmaskGenerator.generate(glyphs.data(), glyphs.size());
                msdfgen::BitmapConstRef<uint8_t> hardmask = hardmaskGenerator.atlasStorage();
                const auto hardmaskInverted = InvertBitmap<1>(hardmask);

                ImmediateAtlasGenerator<float, 4, mtsdfGenerator, BitmapAtlasStorage<uint8_t, 4>> mtsdfGenerator(width, height);
                mtsdfGenerator.setThreadCount(8);

                mtsdfGenerator.generate(glyphs.data(), glyphs.size());
                msdfgen::BitmapConstRef<uint8_t, 4> mtsdf = mtsdfGenerator.atlasStorage();
                const auto mtsdfInverted = InvertBitmap<4>(mtsdf);

                fontc->Atlas.Info.PxRange = pxRange;
                fontc->Atlas.Info.AscenderY = fontGeometry.getMetrics().ascenderY;
                fontc->Atlas.Info.DescenderY = fontGeometry.getMetrics().descenderY;
                fontc->Atlas.Info.Width = width;
                fontc->Atlas.Info.Height = height;

                fontc->Atlas.HardmaskTexture = Texture2D::Create(
                    s_GraphicsContext,
                    EImageFormat::R8_UNORM,
                    width, height,
                    hardmaskInverted.data()
                );

                fontc->Atlas.MTSDFTexture = Texture2D::Create(
                    s_GraphicsContext,
                    EImageFormat::R8G8B8A8_UNORM,
                    width, height,
                    mtsdfInverted.data()
                );

                for (auto& glyph : glyphs)
                {
                    SGlyph glyphc;
                    glyphc.Unicode = glyph.getCodepoint();
                    glyphc.Advance = glyph.getAdvance();

                    double pl, pb, pr, pt;
                    glyph.getQuadPlaneBounds(pl, pb, pr, pt);
                    glyphc.PlaneBounds = SRect{
                        { float(pl),  float(pb) },
                        { float(pr), float(pt) }
                    };

                    double al, ab, ar, at;
                    glyph.getQuadAtlasBounds(al, ab, ar, at);
                    glyphc.AtlasBounds = SRect{
                        { float(al), float(ab) },
                        { float(ar), float(at) }
                    };

                    fontc->Glyphs.push_back(glyphc);
                }

                msdfgen::destroyFont(font);

                s_Fonts[name] = std::move(fontc);
                return s_Fonts[name];
            }

            msdfgen::deinitializeFreetype(ft);
        }

        EE_CORE_FATAL("Cannot initialize FreeType!")
        return nullptr;
    }

    Ref<SFont> FontManager::LoadPrecompiled(
        const std::filesystem::path& directory,
        const std::string& name
    )
    {
        if (is_directory(directory))
        {
            try
            {
                const auto fontJsonPath = directory / (name + ".json");
                const auto fontAtlasPath = directory / (name + ".png");

                if (!FileExists(fontJsonPath))
                {
                    EE_CORE_ERROR("Cannot find {0}.json file! [Path = {1}]", name, directory.string())
                    return nullptr;
                }

                if (!FileExists(fontAtlasPath))
                {
                    EE_CORE_ERROR("Cannot find {0}.png file! [Path = {1}]", name, directory.string())
                    return nullptr;
                }

                Ref<SFont> font = CreateRef<SFont>();
                font->Name = name;

                // Load and parse JSON
                simdjson::ondemand::parser parser;
                const auto json = simdjson::padded_string::load(fontJsonPath.string().c_str());
                simdjson::ondemand::document fontData = parser.iterate(json);

                font->Atlas = LoadAtlas(fontData, fontAtlasPath);
                font->Glyphs = LoadGlyphs(fontData);

                s_Fonts[name] = std::move(font);
                return s_Fonts[name];
            }
            catch (const std::filesystem::filesystem_error&)
            {
                EE_CORE_ERROR("Cannot access font directory! [Path = {0}]", directory.string())
                return nullptr;
            }
            catch (const std::exception& error)
            {
                EE_CORE_ERROR("Error trying to load font: \"{0}\", [Path = {1}, Font = {2}]", error.what(), directory.string(), name)
                return nullptr;
            }
        }

        EE_CORE_ERROR("Font path is not a directory! [Path = {0}]", directory.string())
        return nullptr;
    }
}