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
