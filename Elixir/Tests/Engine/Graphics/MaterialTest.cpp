#include <gtest/gtest.h>

#include <Engine/Graphics/Material/Material.h>
#include <Engine/Graphics/Material/MaterialAsset.h>
#include <Engine/Graphics/Material/MaterialGraphDocument.h>
#include <Engine/Graphics/MeshAsset.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace Elixir;

namespace
{
    struct SMaterialTestDirectory
    {
        SMaterialTestDirectory()
        {
            const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
            Path = std::filesystem::temp_directory_path() / ("elixir-material-assets-" + std::to_string(unique));
            std::filesystem::create_directories(Path);
        }

        ~SMaterialTestDirectory()
        {
            std::error_code ec;
            std::filesystem::remove_all(Path, ec);
        }

        std::filesystem::path Path;
    };
}

TEST(Material, OwnsGraphAndParameterLayout)
{
    auto material = CreateRef<Material>("GraphMaterial");
    material->SetDefault("Tint", SMaterialParam::MakeVector(glm::vec4(1.0f)));
    material->SetDefault("Strength", SMaterialParam::MakeScalar(0.5f));

    MaterialGraph graph;
    SMaterialNode color;
    color.Type = EMaterialNodeType::Constant;
    color.OutputType = EGraphValueType::Float3;
    graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode(color));
    material->SetGraph(std::move(graph), {
        { "Tint", SMaterialParam::EType::Vector, 0 },
        { "Strength", SMaterialParam::EType::Scalar, 1 },
    });

    ASSERT_TRUE(material->HasGraph());
    ASSERT_NE(material->GetGraph(), nullptr);
    ASSERT_EQ(material->GetGraphParameters().size(), 2u);
    EXPECT_EQ(material->GetGraphParameters()[1].Name, "Strength");
    EXPECT_FALSE(material->GetGraph()->Validate().HasErrors());
}

TEST(MaterialInstance, PacksGraphDefaultsAndOverrides)
{
    auto material = CreateRef<Material>("GraphMaterial");
    material->SetDefault("Tint", SMaterialParam::MakeVector(glm::vec4(0.1f, 0.2f, 0.3f, 1.0f)));
    material->SetDefault("Strength", SMaterialParam::MakeScalar(0.5f));
    material->SetGraph(MaterialGraph{}, {
        { "Tint", SMaterialParam::EType::Vector, 0 },
        { "Strength", SMaterialParam::EType::Scalar, 1 },
    });

    MaterialInstance instance(material);
    instance.SetScalar("Strength", 0.8f);

    glm::vec4 params[4] = {};
    EXPECT_EQ(instance.CollectGraphParams(params, 4), 2u);
    EXPECT_EQ(params[0], glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    EXPECT_FLOAT_EQ(params[1].x, 0.8f);
}

TEST(MaterialInstance, PacksMissingTextureAsAnInvalidBindlessIndex)
{
    SMaterialGraphDocument document;
    SMaterialGraphNode texture;
    texture.Id = 1;
    texture.Type = EMaterialNodeType::TextureParameter;
    texture.OutputType = EGraphValueType::Float3;
    std::snprintf(texture.Param, sizeof(texture.Param), "Albedo");
    document.Nodes.push_back(texture);
    document.Channels[(int)EMaterialChannel::BaseColor] = texture.Id;
    document.NextId = 2;

    const Ref<Material> material = document.BuildMaterial("TextureMaterial");
    ASSERT_EQ(material->GetGraphParameters().size(), 1u);
    EXPECT_EQ(material->GetGraphParameters()[0].Type, SMaterialParam::EType::Texture);
    ASSERT_NE(material->GetDefault("Albedo"), nullptr);
    EXPECT_EQ(material->GetDefault("Albedo")->Type, SMaterialParam::EType::Texture);
    EXPECT_EQ(material->GetDefault("Albedo")->TextureRef, nullptr);

    MaterialInstance instance(material);
    glm::vec4 params[1] = {};
    EXPECT_EQ(instance.CollectGraphParams(params, 1), 1u);
    uint32_t encodedIndex = 0;
    std::memcpy(&encodedIndex, &params[0].x, sizeof(encodedIndex));
    EXPECT_EQ(encodedIndex, 0xffffffffu);
}

TEST(MaterialInstance, ReparentKeepsOnlyCompatibleOverrides)
{
    auto first = CreateRef<Material>("First");
    first->SetDefault("Shared", SMaterialParam::MakeScalar(0.25f));
    first->SetDefault("Removed", SMaterialParam::MakeScalar(1.0f));
    MaterialInstance instance(first);
    instance.SetScalar("Shared", 0.75f);
    instance.SetScalar("Removed", 2.0f);

    auto second = CreateRef<Material>("Second");
    second->SetDefault("Shared", SMaterialParam::MakeScalar(0.5f));
    instance.SetParent(second);

    EXPECT_FLOAT_EQ(instance.GetScalar("Shared"), 0.75f);
    EXPECT_EQ(instance.GetParameter("Removed"), nullptr);
}

TEST(MaterialInstance, AppliesOnlyCompatiblePreparedOverrides)
{
    auto material = CreateRef<Material>("GraphMaterial");
    material->SetDefault("Strength", SMaterialParam::MakeScalar(0.5f));
    material->SetDefault("Tint", SMaterialParam::MakeVector(glm::vec4(1.0f)));

    MaterialInstance target(material);
    MaterialInstance prepared(material);
    prepared.SetScalar("Strength", 0.8f);
    prepared.SetScalar("Tint", 0.25f);
    prepared.SetScalar("Unknown", 1.0f);

    target.ApplyCompatibleOverridesFrom(prepared);

    EXPECT_FLOAT_EQ(target.GetScalar("Strength"), 0.8f);
    EXPECT_EQ(target.GetVector("Tint"), glm::vec4(1.0f));
    EXPECT_EQ(target.GetParameter("Unknown"), nullptr);
}

TEST(MaterialInstance, StaticOverrideSelectsAGraphPermutation)
{
    SMaterialGraphDocument document;

    SMaterialGraphNode trueValue;
    trueValue.Id = 1;
    trueValue.Type = EMaterialNodeType::Constant;
    trueValue.OutputType = EGraphValueType::Float3;
    trueValue.Constant = glm::vec4(0.123f, 0.0f, 0.0f, 1.0f);

    SMaterialGraphNode falseValue = trueValue;
    falseValue.Id = 2;
    falseValue.Constant = glm::vec4(0.0f, 0.456f, 0.0f, 1.0f);

    SMaterialGraphNode condition;
    condition.Id = 3;
    condition.Type = EMaterialNodeType::StaticBoolParameter;
    condition.OutputType = EGraphValueType::Float;
    condition.Constant.x = 0.0f;
    std::snprintf(condition.Param, sizeof(condition.Param), "UseTrueValue");

    SMaterialGraphNode staticSwitch;
    staticSwitch.Id = 4;
    staticSwitch.Type = EMaterialNodeType::StaticSwitch;
    staticSwitch.InputCount = 3;
    staticSwitch.Inputs[0] = trueValue.Id;
    staticSwitch.Inputs[1] = falseValue.Id;
    staticSwitch.Inputs[2] = condition.Id;

    document.Nodes = { trueValue, falseValue, condition, staticSwitch };
    document.Channels[(int)EMaterialChannel::BaseColor] = staticSwitch.Id;
    document.NextId = 5;

    const Ref<Material> material = document.BuildMaterial("StaticMaterial");
    MaterialInstance instance(material);
    const std::string defaultHlsl = instance.BuildGraphVariant().GenerateHLSL();
    EXPECT_NE(defaultHlsl.find("0.456000"), std::string::npos);
    EXPECT_EQ(defaultHlsl.find("0.123000"), std::string::npos);

    const size_t defaultKey = instance.StaticVariantKey();
    instance.SetStaticBool("UseTrueValue", true);
    const std::string overriddenHlsl = instance.BuildGraphVariant().GenerateHLSL();
    EXPECT_NE(overriddenHlsl.find("0.123000"), std::string::npos);
    EXPECT_EQ(overriddenHlsl.find("0.456000"), std::string::npos);
    EXPECT_NE(instance.StaticVariantKey(), defaultKey);
}

TEST(MaterialAsset, RoundTripsGraphMaterialAndInstance)
{
    SMaterialTestDirectory directory;
    const auto materialPath = directory.Path / "paint.material.json";
    const auto instancePath = directory.Path / "paint.matinstance.json";

    // No editor anywhere in this test: the document is the authored graph, and the
    // asset layer reads and writes it without the authoring tool.
    SMaterialGraphDocument document;
    SMaterialGraphNode node;
    node.Id = 1;
    node.Type = EMaterialNodeType::ParamScalar;
    node.OutputType = EGraphValueType::Float;
    node.Constant = glm::vec4(0.5f);
    std::snprintf(node.Param, sizeof(node.Param), "Strength");
    document.Nodes.push_back(node);
    document.Channels[(int)EMaterialChannel::BaseColor] = node.Id;

    SMaterialGraphNode staticNode;
    staticNode.Id = 2;
    staticNode.Type = EMaterialNodeType::StaticBoolParameter;
    staticNode.OutputType = EGraphValueType::Float;
    staticNode.Constant.x = 0.0f;
    std::snprintf(staticNode.Param, sizeof(staticNode.Param), "UseClearCoat");
    document.Nodes.push_back(staticNode);

    SMaterialGraphNode textureNode;
    textureNode.Id = 3;
    textureNode.Type = EMaterialNodeType::TextureParameter;
    textureNode.OutputType = EGraphValueType::Float3;
    std::snprintf(textureNode.Param, sizeof(textureNode.Param), "DetailTexture");
    document.Nodes.push_back(textureNode);
    document.NextId = 4;

    const Ref<Material> material = document.BuildMaterial("paint", Material::CreateStandardPBR());
    ASSERT_NE(material->GetDocument(), nullptr); // a built material carries its graph
    ASSERT_TRUE(MaterialAsset::SaveMaterial(materialPath, *material));

    std::ifstream materialFile(materialPath);
    const std::string materialDocument{
        std::istreambuf_iterator<char>(materialFile), std::istreambuf_iterator<char>() };
    EXPECT_NE(materialDocument.find("\"nodes\""), std::string::npos);
    EXPECT_EQ(materialDocument.find("\"targetMaterial\""), std::string::npos);
    EXPECT_EQ(materialDocument.find("\"paramOverrides\""), std::string::npos);

    MaterialInstance instance(material);
    instance.SetName("Body Paint");
    instance.SetScalar("Strength", 0.8f);
    instance.SetVector("BaseColorFactor", glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    instance.SetTexture("DetailTexture", nullptr);
    const size_t defaultVariant = instance.StaticVariantKey();
    instance.SetStaticBool("UseClearCoat", true);
    EXPECT_NE(instance.StaticVariantKey(), defaultVariant);
    ASSERT_TRUE(MaterialAsset::SaveInstance(instancePath, instance, materialPath));

    const Ref<MaterialInstance> loaded = MaterialAsset::LoadInstance(instancePath);
    ASSERT_NE(loaded, nullptr);
    ASSERT_NE(loaded->GetParent(), nullptr);
    EXPECT_TRUE(loaded->GetParent()->HasGraph());
    EXPECT_EQ(loaded->GetName(), "Body Paint");
    EXPECT_FLOAT_EQ(loaded->GetScalar("Strength"), 0.8f);
    EXPECT_EQ(loaded->GetVector("BaseColorFactor"), glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    ASSERT_TRUE(loaded->GetOverrides().contains("DetailTexture"));
    EXPECT_EQ(loaded->GetTexture("DetailTexture"), nullptr);
    EXPECT_TRUE(loaded->GetStaticBool("UseClearCoat"));

    // The graph survives the round trip as an editable document, not just as compiled
    // output -- that is what lets the editor reopen a saved material.
    const SMaterialGraphDocument* reloaded = loaded->GetParent()->GetDocument();
    ASSERT_NE(reloaded, nullptr);
    ASSERT_EQ(reloaded->Nodes.size(), 3u);
    EXPECT_EQ(reloaded->Nodes[0].Type, EMaterialNodeType::ParamScalar);
    EXPECT_STREQ(reloaded->Nodes[0].Param, "Strength");
    EXPECT_EQ(reloaded->Nodes[2].Type, EMaterialNodeType::TextureParameter);
    EXPECT_STREQ(reloaded->Nodes[2].Param, "DetailTexture");
    EXPECT_EQ(reloaded->Channels[(int)EMaterialChannel::BaseColor], node.Id);
}

TEST(MaterialInstance, StaticVariantKeyIsIndependentOfTheGraphRevision)
{
    SMaterialGraphDocument document;
    SMaterialGraphNode condition;
    condition.Id = 1;
    condition.Type = EMaterialNodeType::StaticBoolParameter;
    condition.OutputType = EGraphValueType::Float;
    condition.Constant = glm::vec4(1.0f);
    std::snprintf(condition.Param, sizeof(condition.Param), "UseDetail");
    document.Nodes.push_back(condition);
    document.Channels[(int)EMaterialChannel::BaseColor] = condition.Id;
    document.NextId = 2;

    const Ref<Material> before = document.BuildMaterial("paint");
    MaterialInstance instance(before);
    instance.SetStaticBool("UseDetail", false);

    // Editing the graph mints a new revision.
    SMaterialGraphDocument edited = document;
    SMaterialGraphNode extra;
    extra.Id = 2;
    extra.Type = EMaterialNodeType::Constant;
    edited.Nodes.push_back(extra);
    edited.NextId = 3;

    const Ref<Material> after = edited.BuildMaterial("paint");
    MaterialInstance moved(after);
    moved.SetStaticBool("UseDetail", false);

    EXPECT_NE(instance.GraphRevision(), moved.GraphRevision());

    // ...but the same static selection keeps its key. Were the revision folded back
    // into the key, every edit would mint keys nothing asks for again and the
    // renderer's variant cache would grow without bound.
    EXPECT_EQ(instance.StaticVariantKey(), moved.StaticVariantKey());

    // The key still tells the permutations of one revision apart.
    MaterialInstance other(after);
    other.SetStaticBool("UseDetail", true);
    EXPECT_EQ(other.GraphRevision(), moved.GraphRevision());
    EXPECT_NE(other.StaticVariantKey(), moved.StaticVariantKey());
}

TEST(MaterialGraphDocument, RoundTripsAWholeNumberedAlphaCutoff)
{
    SMaterialTestDirectory directory;
    const auto path = directory.Path / "masked.material.json";

    // 1.0f serialises as the integer token "1", so this pins the cutoff surviving a
    // round trip rather than quietly falling back to its default.
    SMaterialGraphDocument document;
    document.BlendMode = 1; // Masked
    document.AlphaCutoff = 1.0f;
    ASSERT_TRUE(MaterialGraphDocument::Save(path, document));

    const auto loaded = MaterialGraphDocument::Load(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->BlendMode, 1);
    EXPECT_FLOAT_EQ(loaded->AlphaCutoff, 1.0f);
}

TEST(MeshAsset, RoundTripsSourceAndMaterialSlots)
{
    SMaterialTestDirectory directory;
    const auto meshPath = directory.Path / "car.mesh.json";
    const auto modelPath = directory.Path / "car.gltf";
    const auto firstInstance = directory.Path / "paint.matinstance.json";
    const auto secondInstance = directory.Path / "glass.matinstance.json";

    SMeshAssetDescription description;
    description.Source = modelPath;
    description.Materials = { { 7, "Glass", secondInstance }, { 2, "Paint", firstInstance } };
    ASSERT_TRUE(MeshAsset::Save(meshPath, description));

    const auto loaded = MeshAsset::Load(meshPath);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->Source.lexically_normal(), modelPath.lexically_normal());
    ASSERT_EQ(loaded->Materials.size(), 2u);
    EXPECT_EQ(loaded->Materials[0].Slot, 2u);
    EXPECT_EQ(loaded->Materials[0].Name, "Paint");
    EXPECT_EQ(loaded->Materials[0].Material.lexically_normal(), firstInstance.lexically_normal());
    EXPECT_EQ(loaded->Materials[1].Slot, 7u);
    EXPECT_EQ(loaded->Materials[1].Name, "Glass");
    EXPECT_EQ(loaded->Materials[1].Material.lexically_normal(), secondInstance.lexically_normal());
}

TEST(MeshAsset, ReadsSlotsWrittenBeforeNamesExisted)
{
    SMaterialTestDirectory directory;
    const auto meshPath = directory.Path / "legacy.mesh.json";
    {
        std::ofstream out(meshPath);
        out << R"({ "version": 1, "assetType": "Mesh", "source": "car.gltf",)"
            << R"( "materials": [ { "slot": 3, "asset": "paint.matinstance.json" } ] })";
    }

    const auto loaded = MeshAsset::Load(meshPath);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->Materials.size(), 1u);
    EXPECT_EQ(loaded->Materials[0].Slot, 3u);
    EXPECT_TRUE(loaded->Materials[0].Name.empty());
    EXPECT_EQ(loaded->Materials[0].Material.lexically_normal(),
        (directory.Path / "paint.matinstance.json").lexically_normal());
}
