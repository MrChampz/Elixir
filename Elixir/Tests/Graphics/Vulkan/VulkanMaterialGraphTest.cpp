#include <gtest/gtest.h>

#include "VulkanTestContext.h"
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/Nodes/FunctionCall.h>
#include <Engine/Materials/Nodes/Add.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/Panner.h>
#include <Engine/Materials/Nodes/Position.h>
#include <Engine/Materials/Nodes/FunctionInput.h>
#include <Engine/Materials/Nodes/StaticSwitch.h>
#include <Engine/Materials/Nodes/TextureSample.h>
#include <Engine/Materials/Nodes/TexCoord.h>

#include <Engine/Aether/Manager.h>
#include <Engine/Aether/Effect/MaterialFactory.h>
#include <Engine/Graphics/TextureLoader.h>
#include <Engine/Materials/Nodes/TextureAtlasSample.h>
#include <Engine/Materials/Nodes/ParticleTime.h>
#include <Engine/Materials/Nodes/SceneColor.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/Rendering/MaterialRenderProxy.h>
#include <Engine/Materials/Rendering/FrameTable.h>
using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;
using namespace Elixir::Materials::Compilation;

namespace
{
    /** Copies shader templates to an isolated compiler working directory. */
    class ShaderWorkspace
    {
      public:
        ShaderWorkspace() : m_Previous(std::filesystem::current_path())
        {
            namespace fs = std::filesystem;
            auto root = fs::path(__FILE__);
            for (int i = 0; i < 5; ++i)
                root = root.parent_path();
            m_Path =
                fs::temp_directory_path() /
                ("elixir-material-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            fs::create_directories(m_Path / "Shaders");
            fs::copy(
                root / "Shaders/Material",
                m_Path / "Shaders/Material",
                fs::copy_options::recursive
            );
            fs::copy_file(root / "Shaders/Quad.hlsl", m_Path / "Shaders/Quad.hlsl");
            fs::create_directories(m_Path / "Shaders/Aether");
            fs::copy_file(
                root / "Shaders/Aether/ParticleData.hlsl",
                m_Path / "Shaders/Aether/ParticleData.hlsl"
            );
            fs::copy_file(
                root / "Shaders/Aether/Math.hlsl",
                m_Path / "Shaders/Aether/Math.hlsl"
            );
            fs::current_path(m_Path);
        }
        ~ShaderWorkspace()
        {
            std::filesystem::current_path(m_Previous);
            std::filesystem::remove_all(m_Path);
        }

      private:
        std::filesystem::path m_Previous;
        std::filesystem::path m_Path;
    };
}

TEST(VulkanMaterialGraphTest, CompilesFunctionsStaticChoicesAndVertexTextureSampling)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait(
        [&]
        {
            ShaderWorkspace workspace;
            ShaderLoader loader(context);
            /** Select the shader paths covered by material compilation. */
            struct SConfiguration
            {
                EMaterialUsage Usage;
                uint32_t UVChannel;
                bool TangentSpace;
                EMaterialShadingModel ShadingModel;
            };
            const std::array configurations{
                SConfiguration{
                    EMaterialUsage::Surface, 0u, true, EMaterialShadingModel::Lit
                },
                SConfiguration{
                    EMaterialUsage::Surface, 1u, false, EMaterialShadingModel::Lit
                },
                SConfiguration{
                    EMaterialUsage::Surface, 0u, true, EMaterialShadingModel::ClearCoat
                },
                SConfiguration{
                    EMaterialUsage::Surface, 1u, false, EMaterialShadingModel::ClearCoat
                },
                SConfiguration{
                    EMaterialUsage::Surface, 0u, true, EMaterialShadingModel::Transmission
                },
                SConfiguration{
                    EMaterialUsage::Surface, 1u, false, EMaterialShadingModel::Transmission
                },
                SConfiguration{
                    EMaterialUsage::Particle, 0u, true, EMaterialShadingModel::Lit
                },
                SConfiguration{
                    EMaterialUsage::Particle, 1u, false, EMaterialShadingModel::Lit
                },
                SConfiguration{
                    EMaterialUsage::Particle, 0u, true, EMaterialShadingModel::Unlit
                }
            };
            for (const auto& configuration : configurations)
            {
                const auto usage = configuration.Usage;
                const auto uvChannel = configuration.UVChannel;
                MaterialGraph functionGraph;
                const auto texture = functionGraph.AddNode<TextureSample>(
                    "Texture",
                    ETextureSampleType::Mask,
                    STextureSampleSettings{
                        .Address = ETextureAddress::Clamp,
                        .Filter = ETextureFilter::Point,
                        .MipMode = ETextureMipMode::Level
                    }
                );
                const auto panner = functionGraph.AddNode<Panner>(glm::vec2(.1f));
                const auto coordinates = functionGraph.AddNode<TexCoord>(uvChannel);
                functionGraph.Connect(coordinates, panner, 0);
                functionGraph.Connect(panner, texture, 0);
                auto functionOutput = texture;
                for (int i = 0; i < 20; ++i)
                {
                    const auto add = functionGraph.AddNode<Add>();
                    functionGraph.Connect(functionOutput, add, 0);
                    functionGraph.Connect(functionOutput, add, 1);
                    functionOutput = add;
                }
                const auto function = CreateRef<MaterialFunction>(
                    std::move(functionGraph),
                    std::vector<SMaterialFunctionInput>{},
                    functionOutput
                );
                MaterialGraph wrapperGraph;
                const auto first = wrapperGraph.AddNode<FunctionCall>(function);
                const auto second = wrapperGraph.AddNode<FunctionCall>(function);
                const auto sum = wrapperGraph.AddNode<Add>();
                wrapperGraph.Connect(first, sum, 0);
                wrapperGraph.Connect(second, sum, 1);
                const auto wrapper = CreateRef<MaterialFunction>(
                    std::move(wrapperGraph),
                    std::vector<SMaterialFunctionInput>{},
                    sum
                );
                MaterialGraph graph;
                const auto call = graph.AddNode<FunctionCall>(wrapper);
                const auto choice = graph.AddNode<StaticSwitch>("Enabled", true);
                graph.Connect(call, choice, 0);
                graph.SetChannel(EMaterialChannel::BaseColor, choice);
                const auto normal = graph.AddNode<Constant>(
                    glm::vec4(0.2f, 0.7f, 1.0f, 0.0f),
                    EMaterialValueType::Float3
                );
                graph.SetChannel(EMaterialChannel::Normal, normal);
                if (configuration.ShadingModel == EMaterialShadingModel::ClearCoat)
                    graph.SetChannel(EMaterialChannel::ClearCoatBottomNormal, normal);
                if (usage == EMaterialUsage::Surface)
                {
                    const auto position = graph.AddNode<Position>();
                    const auto offset = graph.AddNode<Add>();
                    graph.Connect(position, offset, 0);
                    graph.Connect(choice, offset, 1);
                    graph.SetChannel(EMaterialChannel::WorldPositionOffset, offset);
                }
                const auto material = CreateRef<Material>("CompiledFeatures", usage);
                material->SetTangentSpaceNormal(configuration.TangentSpace);
                material->SetShadingModel(configuration.ShadingModel);
                ASSERT_TRUE(material->DefineParameter(
                    "Texture",
                    {.Kind = EMaterialParameterKind::Texture,
                     .DefaultValue = SMaterialParameter::MakeTexture(nullptr)}
                ));
                material->SetGraph(std::move(graph));
                for (const bool enabled : {true, false})
                {
                    const auto result =
                        Compiler::Compile(&loader, *material, {{"Enabled", enabled}});
                    ASSERT_TRUE(result) << result.Diagnostics;
                    const auto shader = usage == EMaterialUsage::Surface
                                            ? result.Material->SurfaceShader
                                            : result.Material->ParticleSpriteShader;
                    ASSERT_TRUE(shader);
                    EXPECT_EQ(shader->HasBinding("pointClamp"), enabled);
                    if (usage == EMaterialUsage::Particle)
                    {
                        ASSERT_TRUE(result.Material->ParticleMeshShader);
                        ASSERT_TRUE(result.Material->ParticleRibbonShader);
                        ASSERT_TRUE(result.Material->ParticleStrandShader);
                    }
                }
            }
        }
    ));
}


TEST(VulkanMaterialGraphTest, CompilesMigratedParticleEffectsForEveryGeometry)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        const auto root = std::filesystem::path(__FILE__).parent_path().parent_path()
            .parent_path().parent_path().parent_path();
        ShaderWorkspace workspace;
        TextureLoader::Initialize(context);
        ShaderLoader loader(context);
        for (const auto* name : {"BlendModeTest", "FlipbookSmokeTest", "GradientTest",
            "NormalLightTest", "DistortionTest", "NS_Explosion_01", "N_RealExp_00_A",
            "CinematicFX_Showcase"})
        {
            const auto system = Aether::Manager::LoadEffect(
                root / "Assets/VFX" / (std::string(name) + ".json")
            );
            ASSERT_TRUE(system) << name;
            for (const auto& emitter : system->GetEmitters())
            {
                ASSERT_TRUE(emitter->GetMaterialDescription()) << emitter->GetName();
                auto description = *emitter->GetMaterialDescription();
                for (auto* path : {&description.BaseColorTexturePath,
                    &description.EmissiveTexturePath, &description.ColorRampTexturePath,
                    &description.NormalTexturePath, &description.RefractionTexturePath})
                {
                    if (path->empty()) continue;
                    *path = (root / *path).string();
                    ASSERT_TRUE(std::filesystem::exists(*path)) << *path;
                }
                const auto material = Aether::Effect::CreateMaterial(
                    emitter->GetName(),
                    description
                );
                ASSERT_TRUE(material);
                EXPECT_EQ(material->GetShadingModel(), description.ShadingModel);
                EXPECT_EQ(
                    material->GetGraph().HasChannel(EMaterialChannel::Normal),
                    !description.NormalTexturePath.empty()
                );
                const auto result = Compiler::Compile(&loader, *material);
                ASSERT_TRUE(result) << name << ": " << emitter->GetName() << " "
                    << result.Diagnostics;
                for (const auto& shader : {result.Material->ParticleSpriteShader,
                    result.Material->ParticleMeshShader, result.Material->ParticleRibbonShader,
                    result.Material->ParticleStrandShader})
                {
                    ASSERT_TRUE(shader);
                    EXPECT_EQ(shader->HasBinding("sceneColorTexture"),
                        !description.RefractionTexturePath.empty());
                }
            }
        }
    }));
}

TEST(VulkanMaterialGraphTest, OrdinarySurfaceMaterialsDoNotRequireSceneColorBindings)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait(
        [&]
        {
            ShaderWorkspace workspace;
            ShaderLoader loader(context);
            Material material("Ordinary surface", EMaterialUsage::Surface);
            MaterialGraph graph;
            const auto color = graph.AddNode<Constant>(
                glm::vec4(1.0f),
                EMaterialValueType::Float3
            );
            graph.SetChannel(EMaterialChannel::BaseColor, color);
            material.SetGraph(std::move(graph));
            const auto result = Compiler::Compile(&loader, material);
            ASSERT_TRUE(result) << result.Diagnostics;
            const auto& shader = result.Material->SurfaceShader;
            ASSERT_TRUE(shader);
            EXPECT_FALSE(shader->HasBinding("sceneColorTexture"));
            EXPECT_FALSE(shader->HasBinding("sceneColorSampler"));
        }
    ));
}

TEST(VulkanMaterialGraphTest, AtlasAndSceneSamplingAlsoCompileForSurfaceMaterials)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        ShaderWorkspace workspace;
        ShaderLoader loader(context);
        Material material("Generic atlas and scene", EMaterialUsage::Surface);
        material.DefineParameter("Atlas", {
            .Kind = EMaterialParameterKind::TextureAtlas,
            .DefaultValue = SMaterialParameter::MakeTextureAtlas(nullptr)
        });
        MaterialGraph graph;
        const auto atlas = graph.AddNode<TextureAtlasSample>(
            "Atlas",
            true
        );
        const auto scene = graph.AddNode<SceneColor>();
        graph.SetChannel(EMaterialChannel::BaseColor, atlas);
        graph.SetChannel(EMaterialChannel::Emissive, scene);
        material.SetGraph(std::move(graph));
        const auto result = Compiler::Compile(&loader, material);
        ASSERT_TRUE(result) << result.Diagnostics;
        EXPECT_TRUE(result.Material->SurfaceShader->HasBinding("sceneColorTexture"));
    }));
}


TEST(VulkanMaterialGraphTest, AtlasResourcesRetainTextureAndOverrideLayoutWithoutRecompiling)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait(
        [&]
        {
            ShaderWorkspace workspace;
            ShaderLoader loader(context);
            std::array<uint8_t, 64> pixels;
            pixels.fill(255);
            STexture2DCreateInfo info;
            info.Format = EImageFormat::R8G8B8A8_UNORM;
            info.Width = info.Height = 4;
            info.InitialData = pixels.data();
            auto texture = Texture2D::Create(context, info);
            auto first = TextureAtlas::Create(texture, {2, 2});
            auto second = TextureAtlas::Create(texture, {4, 2});
            ASSERT_TRUE(first);
            ASSERT_TRUE(second);
            EXPECT_NE(first->GetUUID(), second->GetUUID());
            EXPECT_NE(first->GetUUID(), texture->GetUUID());
            EXPECT_TRUE(TextureAtlas::Create(texture, {3, 2}));
            EXPECT_FALSE(TextureAtlas::Create(texture, {5, 2}));
            EXPECT_FALSE(TextureAtlas::Create(texture, {2, 0}));
            EXPECT_FALSE(TextureAtlas::Create(nullptr, {2, 2}));
            EXPECT_FALSE(TextureAtlas::Create(texture, {0, 0}));
            std::weak_ptr<Texture> retained = texture;
            texture.reset();
            EXPECT_FALSE(retained.expired());
            const auto material =
                CreateRef<Material>("Atlas override", EMaterialUsage::Surface);
            material->DefineParameter(
                "Atlas",
                {.Kind = EMaterialParameterKind::TextureAtlas,
                 .DefaultValue = SMaterialParameter::MakeTextureAtlas(first)}
            );
            material->DefineParameter(
                "Value",
                {.Kind = EMaterialParameterKind::Value,
                 .ValueType = EMaterialValueType::Float,
                 .DefaultValue = SMaterialParameter::MakeScalar(7)}
            );
            MaterialGraph graph;
            const auto age =
                graph.AddNode<Constant>(glm::vec4{0.5f}, EMaterialValueType::Float);
            const auto atlas = graph.AddNode<TextureAtlasSample>(
                "Atlas",
                true,
                ETextureAtlasFrameMode::Normalized
            );
            graph.Connect(age, atlas, 1);
            graph.SetChannel(EMaterialChannel::BaseColor, atlas);
            material->SetGraph(std::move(graph));
            const auto compiled = Compiler::Compile(&loader, *material);
            ASSERT_TRUE(compiled) << compiled.Diagnostics;
            const auto instance = material->CreateInstance();
            const auto original =
                Rendering::MaterialRenderProxy::Create(compiled.Material, *instance);
            ASSERT_TRUE(original);
            ASSERT_TRUE(instance->SetTextureAtlas("Atlas", second));
            EXPECT_EQ(instance->GetTextureAtlas("Atlas"), second);
            const auto replacement =
                Rendering::MaterialRenderProxy::Create(compiled.Material, *instance);
            ASSERT_TRUE(replacement);
            EXPECT_EQ(original->GetCompiledMaterial(), replacement->GetCompiledMaterial());
            EXPECT_EQ(original->GetValues()[0], glm::vec4(2, 2, 0, 0));
            EXPECT_EQ(replacement->GetValues()[0], glm::vec4(4, 2, 0, 0));
            EXPECT_EQ(replacement->GetValues()[1].x, 7);
            EXPECT_EQ(original->GetTextures()[0], second->GetTexture());
            Rendering::FrameTable table(
                4,
                0,
                [](const Ref<Texture>&)
                {
                    return std::optional<uint32_t>{9};
                }
            );
            ASSERT_TRUE(table.Add(*replacement));
            EXPECT_EQ(table.GetData()[0].Values[0], glm::vec4(4, 2, 0, 0));
            EXPECT_EQ(table.GetData()[0].TextureIndices[0], 9u);
            ASSERT_TRUE(instance->SetTextureAtlas("Atlas", nullptr));
            const auto empty =
                Rendering::MaterialRenderProxy::Create(compiled.Material, *instance);
            ASSERT_TRUE(empty);
            EXPECT_FALSE(empty->GetTextures()[0]);
            EXPECT_EQ(empty->GetValues()[0], glm::vec4(1, 1, 0, 0));
        }
    ));
}
