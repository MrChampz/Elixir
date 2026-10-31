#include <gtest/gtest.h>

#include "VulkanTestContext.h"
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/Nodes/FunctionCall.h>
#include <Engine/Materials/Nodes/FunctionInput.h>
#include <Engine/Materials/Nodes/StaticSwitch.h>
#include <Engine/Materials/Nodes/TextureSample.h>

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
            for (const auto usage : {EMaterialUsage::Surface, EMaterialUsage::Particle})
            {
                MaterialGraph functionGraph;
                const auto texture = functionGraph.AddNode<TextureSample>(
                    "Texture",
                    ETextureSampleType::Mask,
                    STextureSampleSettings{
                        .Address = ETextureSampleAddress::Clamp,
                        .Filter = ETextureSampleFilter::Point,
                        .MipMode = ETextureSampleMipMode::Level
                    }
                );
                const auto function = CreateRef<MaterialFunction>(
                    std::move(functionGraph),
                    std::vector<SMaterialFunctionInput>{},
                    texture
                );
                MaterialGraph graph;
                const auto call = graph.AddNode<FunctionCall>(function);
                const auto choice = graph.AddNode<StaticSwitch>("Enabled", true);
                graph.Connect(call, choice, 0);
                graph.SetChannel(EMaterialChannel::BaseColor, choice);
                if (usage == EMaterialUsage::Surface)
                    graph.SetChannel(EMaterialChannel::WorldPositionOffset, choice);
                const auto material = CreateRef<Material>("CompiledFeatures", usage);
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
                    }
                }
            }
        }
    ));
}
