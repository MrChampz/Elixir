#include "epch.h"

#include <Engine/Materials/Compilation/MaterialGraphEmitter.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/Compilation/MaterialShaderSourceBuilder.h>

namespace Elixir::Materials::Compilation
{
    namespace
    {
        std::string ValueExpression(const SCompiledParameter& parameter)
        {
            std::string value = "mat.Values[" + std::to_string(parameter.Slot) + "]";

            switch (parameter.ValueType)
            {
                case EMaterialValueType::Float:
                    return value + ".x";
                case EMaterialValueType::Float2:
                    return value + ".xy";
                case EMaterialValueType::Float3:
                    return value + ".xyz";
                case EMaterialValueType::Float4:
                    return value;
            }

            return value;
        }
    }

    SMaterialEmissionEnvironment MaterialShaderSourceBuilder::CreateEnvironment(
        const EMaterialUsage usage,
        const EShaderStage stage
    )
    {
        SMaterialEmissionEnvironment environment;
        environment.Stage = stage;

        if (stage != EShaderStage::Vertex && stage != EShaderStage::Pixel)
            return environment;

        if (usage == EMaterialUsage::Particle && stage != EShaderStage::Pixel)
            return environment;

        environment.TexCoords = { "input.TexCoord", "input.TexCoord1" };
        environment.Time = "Time";
        environment.TextureArray = usage == EMaterialUsage::Surface ? "textures" : "sprites";
        environment.TextureSamplers = {
            usage == EMaterialUsage::Surface ? "texSampler" : "spriteSampler",
            "linearClamp",
            "pointWrap",
            "pointClamp"
        };

        if (usage == EMaterialUsage::Surface)
        {
            environment.WorldPosition = stage == EShaderStage::Vertex
                ? "worldPos"
                : "input.WorldPos";
        }
        else
        {
            environment.Color = "input.Color";
        }

        if (stage == EShaderStage::Pixel)
        {
            environment.Normal = "N";
            environment.ViewDirection = "V";
        }

        return environment;
    }

    SShaderSourceResult MaterialShaderSourceBuilder::Build(
        const std::string& shaderTemplate,
        const Material& material,
        const SCompiledMaterial& compiled,
        const EShaderStage stage
    )
    {
        SMaterialGraphBindings bindings;
        bindings.StaticValues = compiled.StaticValues;

        for (const auto& parameter : compiled.Parameters)
        {
            if (parameter.Kind == EMaterialParameterKind::Texture)
                bindings.Textures[parameter.Name] =
                    "mat.TextureIndices[" + std::to_string(parameter.Slot) + "]";
            else
                bindings.Values[parameter.Name] = ValueExpression(parameter);
        }

        const auto environment = CreateEnvironment(compiled.GetUsage(), stage);
        MaterialGraphEmitter emitter(material.GetGraph(), environment, bindings);
        const auto body = emitter.GenerateHLSL();

        if (!emitter.GetDiagnostics().empty())
            return { .Diagnostics = emitter.GetDiagnostics() };

        std::string source = shaderTemplate;
        constexpr std::string_view graphMarker = "// __GRAPH_BODY__";

        const auto graphPosition = source.find(graphMarker);
        if (graphPosition == std::string::npos)
        {
            return {
                .Diagnostics = "Material shader template has no graph insertion point."
            };
        }

        source.replace(graphPosition, graphMarker.size(), body);

        constexpr std::string_view shadingMarker = "// __SHADING_MODEL__";
        if (const auto position = source.find(shadingMarker); position != std::string::npos)
        {
            const auto shadingModel = std::to_string(uint8_t(material.GetShadingModel()));
            source.replace(
                position,
                shadingMarker.size(),
                "#define MATERIAL_SHADING_MODEL " + shadingModel
            );
        }

        if (stage == EShaderStage::Vertex && material.GetUsage() == EMaterialUsage::Surface)
        {
            const auto wpo = material.GetGraph()
                .HasChannel(EMaterialChannel::WorldPositionOffset);

            source = "#define GRAPH_HAS_WPO " + std::to_string(wpo ? 1 : 0) + "\n" + source;
        }

        if (source.empty())
            return { .Diagnostics = "Material shader template produced empty source." };

        return { .Source = std::move(source) };
    }
}
