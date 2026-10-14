#include <VFX/Rendering/LeafSettings.h>
#include <gtest/gtest.h>
#include <limits>

using namespace EditorVFX;

TEST(LeafSettingsTest, SeparatesCustomRendererFromSimulationAndPreservesEmitterIndices)
{
    Value effect; std::string error;
    ASSERT_TRUE(Value::Parse(R"({"emitters":[
        {"name":"Cable","renderMode":"Strand","strand":{"widthScale":0.2}},
        {"name":"Ivy","renderMode":"Strand","leafRenderer":{"interval":3,"length":0.13,"width":0.15,"color":[0.08,0.42,0.1,1]}},
        {"name":"Willow","renderMode":"Strand","leafRenderer":{"enabled":false,"shape":"Lanceolate","maturationSegments":4}}]})", effect, error));
    std::vector<std::optional<LeafSettings>> renderers;
    ASSERT_TRUE(ParseLeafRenderers(effect, renderers, error)) << error;
    ASSERT_EQ(renderers.size(), 3u);
    EXPECT_FALSE(renderers[0]);
    ASSERT_TRUE(renderers[1]);
    EXPECT_EQ(renderers[1]->GetLeafCount(40), 12u);
    EXPECT_EQ(renderers[1]->GetLeafCount(2), 0u);
    EXPECT_FLOAT_EQ(renderers[1]->Length, .13f);
    EXPECT_FLOAT_EQ(renderers[1]->Color[1], .42f);
    ASSERT_TRUE(renderers[2]);
    EXPECT_FALSE(renderers[2]->Enabled);
    EXPECT_EQ(renderers[2]->Shape, ELeafShape::Lanceolate);
    EXPECT_FLOAT_EQ(renderers[2]->MaturationSegments, 4);
}

TEST(LeafSettingsTest, RejectsInvalidSettingsWithoutReplacingThePreviousConfiguration)
{
    std::vector<std::optional<LeafSettings>> renderers{LeafSettings{}};
    for (const char* fields : {R"("interval":0)", R"("interval":97)", R"("interval":2.5)",
        R"("length":0)", R"("width":-1)", R"("width":1e100)", R"("maturationSegments":0)",
        R"("color":[0,1,0])", R"("color":[0,1,0,2])", R"("color":[-1,1,0,1])",
        R"("shape":"Unknown")", R"("enabled":1)"})
    {
        SCOPED_TRACE(fields);
        Value effect; std::string error;
        ASSERT_TRUE(Value::Parse(std::string(R"({"emitters":[{"name":"Plant","renderMode":"Strand","leafRenderer":{)") + fields + "}}]}", effect, error));
        EXPECT_FALSE(ParseLeafRenderers(effect, renderers, error));
        EXPECT_FALSE(error.empty());
        ASSERT_EQ(renderers.size(),1u);
        ASSERT_TRUE(renderers[0]);
        EXPECT_FLOAT_EQ(renderers[0]->Width,.1f);
    }
    LeafSettings settings;
    settings.Length=std::numeric_limits<float>::infinity(); EXPECT_FALSE(settings.IsValid());
    settings.Length=.1f; settings.Color[0]=std::numeric_limits<float>::quiet_NaN(); EXPECT_FALSE(settings.IsValid());
}

TEST(LeafSettingsTest, RequiresStrandDataAndSupportsRemovingLeaves)
{
    Value effect; std::string error;
    ASSERT_TRUE(Value::Parse(R"({"emitters":[{"renderMode":"Sprite","leafRenderer":{}}]})", effect, error));
    std::vector<std::optional<LeafSettings>> renderers;
    EXPECT_FALSE(ParseLeafRenderers(effect, renderers, error));
    ASSERT_TRUE(Value::Parse(R"({"emitters":[{"renderMode":"Strand"}]})", effect, error));
    ASSERT_TRUE(ParseLeafRenderers(effect, renderers, error));
    ASSERT_EQ(renderers.size(),1u); EXPECT_FALSE(renderers[0]);
}
