#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialDomain.h>
#include <Engine/Graphics/Material/MaterialGraph.h>

#include <algorithm>

using namespace Elixir;

namespace
{
    bool HasDiagnostic(const SMaterialGraphValidation& validation, const std::string_view text)
    {
        return std::ranges::any_of(validation.Diagnostics,
            [=](const SMaterialGraphDiagnostic& diagnostic)
            {
                return diagnostic.Message.find(text) != std::string::npos;
            });
    }

    MaterialGraph GraphWithOutput(const EMaterialChannel channel)
    {
        MaterialGraph graph;
        SMaterialNode constant;
        constant.Type = EMaterialNodeType::Constant;
        constant.OutputType = EGraphValueType::Float4;
        const uint32_t node = graph.AddNode(constant);
        graph.SetChannel(channel, node);
        return graph;
    }
}

TEST(MaterialDomain, SurfaceDescriptorPreservesTheCurrentRendererContract)
{
    const auto& surface = MaterialDomain::Surface();
    EXPECT_EQ(surface.Domain, EMaterialDomain::Surface);
    EXPECT_TRUE(surface.ShaderContractAvailable);
    EXPECT_EQ(surface.PixelTemplate, "GraphMaterial.ps.hlsl");
    EXPECT_EQ(surface.VertexTemplate, "GraphMaterial.vs.hlsl");
    EXPECT_TRUE(surface.AllowsChannel((uint8_t)EMaterialChannel::BaseColor));
    EXPECT_TRUE(surface.AllowsChannel((uint8_t)EMaterialChannel::WorldPositionOffset));
    EXPECT_TRUE(HasMaterialUsage(surface.AllowedUsages, EMaterialUsage::StaticMesh));
    EXPECT_TRUE(HasMaterialUsage(surface.AllowedUsages, EMaterialUsage::ParticleMesh));
}

TEST(MaterialDomain, RejectsDomainsWithoutARendererContractAndTheirInvalidOutputs)
{
    MaterialGraph graph = GraphWithOutput(EMaterialChannel::BaseColor);
    graph.SetDomain(EMaterialDomain::PostProcess);
    graph.SetUsages(EMaterialUsage::None);

    const auto validation = graph.Validate();
    EXPECT_TRUE(validation.HasErrors());
    EXPECT_TRUE(HasDiagnostic(validation, "does not have a renderer shader contract"));
    EXPECT_TRUE(HasDiagnostic(validation, "BaseColor"));
}

TEST(MaterialDomain, RequiresAUsageForSurfaceMaterials)
{
    MaterialGraph graph = GraphWithOutput(EMaterialChannel::BaseColor);
    graph.SetUsages(EMaterialUsage::None);

    const auto validation = graph.Validate();
    EXPECT_TRUE(validation.HasErrors());
    EXPECT_TRUE(HasDiagnostic(validation, "must select at least one usage"));
}
