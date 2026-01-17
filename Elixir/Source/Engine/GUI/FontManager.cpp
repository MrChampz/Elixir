#include "epch.h"
#include "FontManager.h"

#include "Engine/Graphics/TextureLoader.h"

#include <simdjson.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
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
        auto obj = fontData["atlas"].get_object();

        SAtlas atlas;
        atlas.Info.Width = int(obj["width"]);
        atlas.Info.Height = int(obj["height"]);

        atlas.Texture = TextureLoader::Load(atlasPath, EImageFormat::R8G8B8A8_UNORM);

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

    Ref<SFont> FontManager::Load(const std::filesystem::path& filepath)
    {
        EE_PROFILE_ZONE_SCOPED()

        if (msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype())
        {
            if (msdfgen::FontHandle* font = msdfgen::loadFont(ft, filepath.c_str()))
            {
                const auto name = filepath.stem().string();

                Ref<SFont> fontc = CreateRef<SFont>();
                fontc->Name = name;

                std::vector<GlyphGeometry> glyphs;
                FontGeometry fontGeometry(&glyphs);

                const auto count = fontGeometry.loadCharset(font, 1.0, Charset::ASCII);
                EE_CORE_TRACE("Loaded {0} glyphs [Font = {1}].", count, name)

                for (auto& glyph : glyphs)
                {
                    constexpr double maxCornerAngle = 3.0;
                    glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
                }

                TightAtlasPacker packer;
                packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
                packer.setMinimumScale(256.0);
                packer.setPixelRange(6.0);
                packer.setMiterLimit(1.0);

                const auto remaining = packer.pack(glyphs.data(), glyphs.size());
                EE_CORE_ASSERT(remaining == 0, "Failed to pack glyphs!")

                int width = 0, height = 0;
                packer.getDimensions(width, height);

                ImmediateAtlasGenerator<float, 4, mtsdfGenerator, BitmapAtlasStorage<uint8_t, 4>> generator(width, height);
                GeneratorAttributes attributes;
                generator.setAttributes(attributes);
                generator.setThreadCount(8);

                generator.generate(glyphs.data(), glyphs.size());

                msdfgen::BitmapConstRef<uint8_t, 4> atlas = generator.atlasStorage();

                fontc->Atlas.Info.AscenderY = fontGeometry.getMetrics().ascenderY;
                fontc->Atlas.Info.DescenderY = fontGeometry.getMetrics().descenderY;
                fontc->Atlas.Info.Width = atlas.width;
                fontc->Atlas.Info.Height = atlas.height;

                fontc->Atlas.Texture = Texture2D::Create(
                    s_GraphicsContext,
                    EImageFormat::R8G8B8A8_UNORM,
                    atlas.width, atlas.height,
                    atlas.pixels
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

        // if (is_directory(directory))
        // {
        //     try
        //     {
        //         // const auto fontJsonPath = directory / (name + ".json");
        //         // const auto fontAtlasPath = directory / (name + ".png");
        //         //
        //         // if (!FileExists(fontJsonPath))
        //         // {
        //         //     EE_CORE_ERROR("Cannot find {0}.json file! [Path = {1}]", name, directory.string())
        //         //     return nullptr;
        //         // }
        //         //
        //         // if (!FileExists(fontAtlasPath))
        //         // {
        //         //     EE_CORE_ERROR("Cannot find {0}.png file! [Path = {1}]", name, directory.string())
        //         //     return nullptr;
        //         // }
        //         //
        //         // Ref<SFont> font = CreateRef<SFont>();
        //         // font->Name = name;
        //         //
        //         // // Load and parse JSON
        //         // simdjson::ondemand::parser parser;
        //         // const auto json = simdjson::padded_string::load(fontJsonPath.c_str());
        //         // simdjson::ondemand::document fontData = parser.iterate(json);
        //         //
        //         // font->Atlas = LoadAtlas(fontData, fontAtlasPath);
        //         // font->Glyphs = LoadGlyphs(fontData);
        //         //
        //         // s_Fonts[name] = std::move(font);
        //         // return s_Fonts[name];
        //     }
        //     catch (const std::filesystem::filesystem_error&)
        //     {
        //         EE_CORE_ERROR("Cannot access font directory! [Path = {0}]", directory.string())
        //         return nullptr;
        //     }
        //     catch (const std::exception& error)
        //     {
        //         EE_CORE_ERROR("Error trying to load font: \"{0}\", [Path = {1}]", error.what(), directory.string())
        //         return nullptr;
        //     }
        // }
        //
        // EE_CORE_ERROR("Font path is not a directory! [Path = {0}]", directory.string())
        // return nullptr;
    }
}