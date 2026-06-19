#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Aether/Effect.h>
#include <Engine/Aether/System.h>
using namespace Elixir;
using namespace Elixir::Aether;

#include <fstream>

namespace
{
    bool HasOp(const SGPUSystem& system, EParticleOp type, EParticleAttribute target)
    {
        for (const auto& op : system.Ops)
        {
            if (op.Type == type && op.Target == target)
                return true;
        }
        return false;
    }
}

class EffectLoaderTest : public Test
{
  protected:
    std::filesystem::path WriteAsset(const std::string& filename, std::string_view contents)
    {
        const auto path = std::filesystem::temp_directory_path() / filename;
        std::ofstream out(path, std::ios::trunc);
        out << contents;
        out.close();
        m_Written.push_back(path);
        return path;
    }

    void TearDown() override
    {
        for (const auto& path : m_Written)
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }

    std::vector<std::filesystem::path> m_Written;
};

TEST_F(EffectLoaderTest, LoadsValidAsset)
{
    const auto path = WriteAsset("valid_effect.json", R"({
        "name": "Test Effect",
        "parameters": { "speed": 5.0, "tint": [0.5, 0.6, 0.7, 1.0] },
        "curves": { "fade": [1.0, 0.5, 0.0] },
        "emitters": [
            {
                "name": "E0",
                "renderMode": "Sprite",
                "maxParticles": 128,
                "spawnRate": "$speed",
                "parameters": { "speed": 12.0 },
                "spawnModules": [
                    { "type": "SetSize", "min": 1.0, "max": 2.0 },
                    { "type": "SetColor", "color": "$tint" }
                ],
                "updateModules": [
                    { "type": "ScaleOverLife", "start": 1.0, "end": 0.0, "curve": "fade", "input": "NormalizedAge" }
                ]
            }
        ]
    })");

    const auto system = LoadEffectFile(path);
    ASSERT_NE(system, nullptr);

    const auto gpu = system->Build();
    EXPECT_EQ(gpu.Name, "Test Effect");
    ASSERT_EQ(gpu.Emitters.size(), 1u);
    EXPECT_EQ(gpu.Emitters[0].RenderMode, EParticleRenderMode::Sprite);
    EXPECT_EQ(gpu.Emitters[0].MaxParticles, 128u);

    // Emitter-scoped "speed" (12.0) overrides the system parameter for spawn rate.
    EXPECT_FLOAT_EQ(gpu.Emitters[0].SpawnRatePerSecond, 12.0f);

    // Regression: the curve must actually be bound to ScaleOverLife, which the
    // old loader dropped silently. A bound curve compiles to a SampleCurve op.
    EXPECT_TRUE(HasOp(gpu, EParticleOp::SampleCurve, EParticleAttribute::Scale));

    EXPECT_NE(FindParameterIndex(gpu.Parameters, "speed"), UINT32_MAX);
    EXPECT_NE(FindParameterIndex(gpu.Parameters, "fade:0"), UINT32_MAX);
}

TEST_F(EffectLoaderTest, ReturnsNullWhenRootIsNotObject)
{
    const auto path = WriteAsset("not_object.json", "[1, 2, 3]");
    EXPECT_EQ(LoadEffectFile(path), nullptr);
}

TEST_F(EffectLoaderTest, ReturnsNullWhenEmittersMissing)
{
    const auto path = WriteAsset("no_emitters.json", R"({ "name": "Broken" })");
    EXPECT_EQ(LoadEffectFile(path), nullptr);
}

TEST_F(EffectLoaderTest, ReturnsNullOnUnknownModuleType)
{
    const auto path = WriteAsset("unknown_module.json", R"({
        "name": "Bad",
        "emitters": [
            {
                "name": "E0",
                "renderMode": "Sprite",
                "maxParticles": 8,
                "spawnRate": 1.0,
                "spawnModules": [ { "type": "DoesNotExist" } ]
            }
        ]
    })");
    EXPECT_EQ(LoadEffectFile(path), nullptr);
}

TEST_F(EffectLoaderTest, ReturnsNullOnUnknownRenderMode)
{
    const auto path = WriteAsset("bad_rendermode.json", R"({
        "name": "Bad",
        "emitters": [
            { "name": "E0", "renderMode": "Hologram", "maxParticles": 8, "spawnRate": 1.0 }
        ]
    })");
    EXPECT_EQ(LoadEffectFile(path), nullptr);
}

TEST_F(EffectLoaderTest, ReturnsNullOnMalformedJson)
{
    const auto path = WriteAsset("malformed.json", R"({ "name": "x", )");
    EXPECT_EQ(LoadEffectFile(path), nullptr);
}

TEST_F(EffectLoaderTest, ReturnsNullWhenFileMissing)
{
    EXPECT_EQ(LoadEffectFile("does_not_exist_12345.json"), nullptr);
}
