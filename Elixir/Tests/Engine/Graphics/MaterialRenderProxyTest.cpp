#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialRenderProxy.h>

#include <cstdio>

using namespace Elixir;

TEST(MaterialRenderProxy, SharesAResourceButKeepsIndependentImmutableValues)
{
    const Ref<Material> material = Material::CreateStandardPBR();
    MaterialInstance red(material);
    red.SetName("Red");
    red.SetVector("BaseColorFactor", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    MaterialInstance blue(material);
    blue.SetName("Blue");
    blue.SetVector("BaseColorFactor", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    const auto resource = MaterialResource::Create(
        red, MaterialShaderPermutation::SurfaceStaticMesh(), EMaterialBlendMode::Opaque);
    const auto redProxy = MaterialRenderProxy::Create(red, resource);
    const auto blueProxy = MaterialRenderProxy::Create(blue, resource);

    ASSERT_NE(resource, nullptr);
    ASSERT_NE(redProxy, nullptr);
    ASSERT_NE(blueProxy, nullptr);
    EXPECT_EQ(redProxy->GetResource(), blueProxy->GetResource());
    EXPECT_EQ(redProxy->GetVector("BaseColorFactor"), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(blueProxy->GetVector("BaseColorFactor"), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    red.SetVector("BaseColorFactor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    EXPECT_EQ(redProxy->GetVector("BaseColorFactor"), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

TEST(MaterialRenderProxy, RejectsAResourceFromAnotherParent)
{
    const Ref<Material> first = Material::CreateStandardPBR();
    const Ref<Material> second = Material::CreateStandardPBR();
    MaterialInstance firstInstance(first);
    MaterialInstance secondInstance(second);

    const auto resource = MaterialResource::Create(
        firstInstance, MaterialShaderPermutation::SurfaceStaticMesh(), EMaterialBlendMode::Opaque);
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(MaterialRenderProxy::Create(secondInstance, resource), nullptr);
}

TEST(MaterialRenderProxy, PacksGraphParametersFromItsSnapshot)
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
    const auto proxy = MaterialRenderProxy::Create(instance);
    ASSERT_NE(proxy, nullptr);

    instance.SetScalar("Strength", 0.2f);
    glm::vec4 params[4] = {};
    EXPECT_EQ(proxy->CollectGraphParams(params, 4), 2u);
    EXPECT_EQ(params[0], glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    EXPECT_FLOAT_EQ(params[1].x, 0.8f);
}

TEST(MaterialResource, DistinguishesStaticVariantsOfOneGraphRevision)
{
    SMaterialGraphDocument document;

    SMaterialGraphNode enabledValue;
    enabledValue.Id = 1;
    enabledValue.Type = EMaterialNodeType::Constant;
    enabledValue.OutputType = EGraphValueType::Float3;
    enabledValue.Constant = glm::vec4(1.0f);

    SMaterialGraphNode disabledValue = enabledValue;
    disabledValue.Id = 2;
    disabledValue.Constant = glm::vec4(0.0f);

    SMaterialGraphNode parameter;
    parameter.Id = 3;
    parameter.Type = EMaterialNodeType::StaticBoolParameter;
    parameter.OutputType = EGraphValueType::Float;
    parameter.Constant.x = 0.0f;
    std::snprintf(parameter.Param, sizeof(parameter.Param), "UseDetail");

    SMaterialGraphNode staticSwitch;
    staticSwitch.Id = 4;
    staticSwitch.Type = EMaterialNodeType::StaticSwitch;
    staticSwitch.OutputType = EGraphValueType::Float3;
    staticSwitch.InputCount = 3;
    staticSwitch.Inputs[0] = enabledValue.Id;
    staticSwitch.Inputs[1] = disabledValue.Id;
    staticSwitch.Inputs[2] = parameter.Id;

    document.Nodes = { enabledValue, disabledValue, parameter, staticSwitch };
    document.Channels[(int)EMaterialChannel::BaseColor] = staticSwitch.Id;
    document.NextId = 5;

    const Ref<Material> material = document.BuildMaterial("StaticMaterial");
    MaterialInstance disabled(material);
    MaterialInstance enabled(material);
    enabled.SetStaticBool("UseDetail", true);

    const auto resource = MaterialResource::Create(
        disabled, MaterialShaderPermutation::SurfaceStaticMesh(), EMaterialBlendMode::Opaque);
    ASSERT_NE(resource, nullptr);
    EXPECT_TRUE(resource->IsCompatible(
        disabled, MaterialShaderPermutation::SurfaceStaticMesh(), EMaterialBlendMode::Opaque));
    EXPECT_FALSE(resource->IsCompatible(
        enabled, MaterialShaderPermutation::SurfaceStaticMesh(), EMaterialBlendMode::Opaque));
    EXPECT_EQ(MaterialRenderProxy::Create(enabled, resource), nullptr);
}
