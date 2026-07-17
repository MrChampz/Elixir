#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <Engine/Graphics/MeshPassProcessor.h>

using namespace Elixir;

namespace
{
    Ref<const MaterialRenderProxy> MakeProxy(
        const float alphaMode = 0.0f, const float alpha = 1.0f)
    {
        const Ref<Material> material = Material::CreateStandardPBR();
        MaterialInstance instance(material);
        instance.SetScalar("AlphaMode", alphaMode);
        instance.SetVector("BaseColorFactor", glm::vec4(1.0f, 1.0f, 1.0f, alpha));
        return MaterialRenderProxy::Create(instance);
    }
}

TEST(MeshPassProcessor, ClassifiesBuiltInMaterials)
{
    EXPECT_EQ(MeshPassProcessor::Classify(nullptr), EMeshPass::BaseOpaque);
    EXPECT_EQ(MeshPassProcessor::Classify(MakeProxy()), EMeshPass::BaseOpaque);
    EXPECT_EQ(MeshPassProcessor::Classify(MakeProxy(1.0f)), EMeshPass::BaseMasked);
    EXPECT_EQ(MeshPassProcessor::Classify(MakeProxy(2.0f, 0.5f)), EMeshPass::Refraction);
    EXPECT_EQ(MeshPassProcessor::Classify(MakeProxy(2.0f, 0.99f)), EMeshPass::BaseOpaque);
}

TEST(MeshPassProcessor, GraphBlendModeOverridesBuiltInClassification)
{
    const auto refractive = MakeProxy(2.0f, 0.5f);

    EXPECT_EQ(MeshPassProcessor::Classify(
        refractive, EMaterialBlendMode::Opaque), EMeshPass::BaseOpaque);
    EXPECT_EQ(MeshPassProcessor::Classify(
        refractive, EMaterialBlendMode::Masked), EMeshPass::BaseMasked);
    EXPECT_EQ(MeshPassProcessor::Classify(
        refractive, EMaterialBlendMode::Translucent), EMeshPass::Translucent);
    EXPECT_EQ(MeshPassProcessor::Classify(
        refractive, EMaterialBlendMode::Additive), EMeshPass::Translucent);
}

TEST(MeshPassProcessor, ExposesPassExecutionRequirements)
{
    EXPECT_TRUE(MeshPassProcessor::IsBasePass(EMeshPass::BaseOpaque));
    EXPECT_TRUE(MeshPassProcessor::IsBasePass(EMeshPass::BaseMasked));
    EXPECT_FALSE(MeshPassProcessor::IsBasePass(EMeshPass::Translucent));
    EXPECT_FALSE(MeshPassProcessor::IsBasePass(EMeshPass::Refraction));

    EXPECT_TRUE(MeshPassProcessor::RequiresSorting(EMeshPass::Translucent));
    EXPECT_TRUE(MeshPassProcessor::RequiresSorting(EMeshPass::Refraction));
    EXPECT_FALSE(MeshPassProcessor::RequiresSorting(EMeshPass::BaseOpaque));

    EXPECT_TRUE(MeshPassProcessor::RequiresSceneColor(EMeshPass::Refraction));
    EXPECT_FALSE(MeshPassProcessor::RequiresSceneColor(EMeshPass::Translucent));
}

TEST(MeshPassProcessor, BuildsAnImmutablePrimitiveCommand)
{
    SModelPrimitive primitive;
    primitive.IndexCount = 42;
    primitive.MaterialIndex = 3;
    primitive.Transform = glm::translate(
        glm::mat4(1.0f), glm::vec3(4.0f, 5.0f, 6.0f));

    const SMeshDrawCommand command = MeshPassProcessor::BuildCommand(
        primitive, MakeProxy(1.0f), nullptr, nullptr);

    EXPECT_EQ(command.Pass, EMeshPass::BaseMasked);
    EXPECT_EQ(command.IndexCount, 42u);
    EXPECT_EQ(command.PushConstants.MaterialIndex, 3u);
    EXPECT_EQ(command.PushConstants.Model, primitive.Transform);
    EXPECT_EQ(command.SortOrigin, glm::vec3(4.0f, 5.0f, 6.0f));
}
