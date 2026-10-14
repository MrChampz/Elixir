#include <VFX/Rendering/BalloonSettings.h>
#include <gtest/gtest.h>
#include <limits>
using namespace EditorVFX;

TEST(BalloonSettingsTest, KeepsRendererSettingsIndependentAndPreservesEmitterIndices)
{
    Value effect; std::string error;
    ASSERT_TRUE(Value::Parse(R"({"emitters":[
        {"renderMode":"Strand"},
        {"renderMode":"Strand","balloonRenderer":{"radius":0.13,"height":0.3,"tilt":0.7,"seed":4294967295,"colorVariation":0.5,"color":[1,0.2,0.3,1]}},
        {"renderMode":"Strand","balloonRenderer":{"enabled":false}}]})",effect,error));
    std::vector<std::optional<BalloonSettings>> settings;
    ASSERT_TRUE(ParseBalloonRenderers(effect,settings,error)) << error;
    ASSERT_EQ(settings.size(),3u);
    EXPECT_FALSE(settings[0]);
    ASSERT_TRUE(settings[1]);
    EXPECT_FLOAT_EQ(settings[1]->Radius,.13f);
    EXPECT_FLOAT_EQ(settings[1]->Height,.3f);
    EXPECT_FLOAT_EQ(settings[1]->Tilt,.7f);
    EXPECT_FLOAT_EQ(settings[1]->Color[1],.2f);
    EXPECT_EQ(settings[1]->Seed,UINT32_MAX);
    ASSERT_TRUE(settings[2]);
    EXPECT_FALSE(settings[2]->Enabled);
}

TEST(BalloonSettingsTest, RejectsInvalidGeometryAndPreservesTheLastConfiguration)
{
    std::vector<std::optional<BalloonSettings>> settings{BalloonSettings{}};
    for (const auto* fields : {R"("radius":0)",R"("height":-1)",R"("height":1e100)",R"("tilt":1.1)",
        R"("colorVariation":-1)",R"("color":[1,0,0])",R"("color":[1,0,0,2])",R"("enabled":1)",R"("seed":-1)"})
    {
        SCOPED_TRACE(fields);
        Value effect; std::string error;
        ASSERT_TRUE(Value::Parse(std::string(R"({"emitters":[{"renderMode":"Strand","balloonRenderer":{)")+fields+"}}]}",effect,error));
        EXPECT_FALSE(ParseBalloonRenderers(effect,settings,error));
        EXPECT_FALSE(error.empty());
        ASSERT_EQ(settings.size(),1u); ASSERT_TRUE(settings[0]); EXPECT_FLOAT_EQ(settings[0]->Radius,.11f);
    }
    BalloonSettings invalid;
    invalid.Radius=std::numeric_limits<float>::infinity(); EXPECT_FALSE(invalid.IsValid());
}

TEST(BalloonSettingsTest, RequiresStrandData)
{
    Value effect; std::string error;
    ASSERT_TRUE(Value::Parse(R"({"emitters":[{"renderMode":"Mesh","balloonRenderer":{}}]})",effect,error));
    std::vector<std::optional<BalloonSettings>> settings;
    EXPECT_FALSE(ParseBalloonRenderers(effect,settings,error));
}

TEST(BalloonSettingsTest, BouquetSampleHasOneCommonRootAndOneBalloonPerPersistentCord)
{
    const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Assets/VFX/StrandBalloons.json";
    Document document; std::string error;
    ASSERT_TRUE(document.Load(path,error)) << error;
    std::vector<std::optional<BalloonSettings>> settings;
    ASSERT_TRUE(ParseBalloonRenderers(document.Root(),settings,error)) << error;
    const auto& emitters = document.Root().Find("emitters")->Array;
    ASSERT_EQ(emitters.size(),settings.size());
    unsigned count = 0;
    std::string commonRoot;
    for (size_t index=0; index<emitters.size(); ++index)
    {
        const auto& emitter = emitters[index];
        ASSERT_TRUE(settings[index]); EXPECT_TRUE(settings[index]->Enabled);
        EXPECT_FALSE(emitter.Find("strand")->Find("endAttachment"));
        EXPECT_EQ(emitter.Find("particleState")->Find("killOnLifetimeExpired")->Text,"false");
        EXPECT_EQ(emitter.Find("spawnRate")->Text,"0");
        const auto capacity = emitter.Find("maxParticles")->Text;
        EXPECT_EQ(emitter.Find("burst")->Find("count")->Text,capacity);
        EXPECT_EQ(emitter.Find("burst")->Find("interval")->Text,"0");
        count += unsigned(std::stoul(capacity));
        const auto& position = emitter.Find("spawnModules")->Array[0];
        EXPECT_EQ(position.String("type"),"SetPositionBox");
        EXPECT_EQ(position.Find("min")->JSON(),position.Find("max")->JSON());
        if (commonRoot.empty()) commonRoot=position.Find("min")->JSON();
        EXPECT_EQ(position.Find("min")->JSON(),commonRoot);
    }
    EXPECT_EQ(count,64u);
}
