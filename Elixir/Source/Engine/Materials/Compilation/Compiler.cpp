#include "epch.h"
#include "Compiler.h"

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialGraph.h>
#include <Engine/Materials/MaterialParameter.h>

namespace Elixir::Materials::Compilation
{
    namespace fs = std::filesystem;

    static const fs::path s_ShadersDir = "./Shaders";
    static const fs::path s_GeneratedDir = s_ShadersDir / "Generated";

    namespace
    {
        fs::path FindDXC()
        {
            if (const char* sdk = std::getenv("VULKAN_SDK"))
            {
                fs::path mac = fs::path(sdk) / "macOS" / "bin" / "dxc";
                if (fs::exists(mac)) return mac;

                const fs::path bin = fs::path(sdk) / "bin" / "dxc";
                if (fs::exists(bin)) return bin;
            }

            return "dxc"; // rely on PATH
        }

        std::string ReadFile(const fs::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in) return {};

            std::stringstream ss;
            ss << in.rdbuf();

            return ss.str();
        }

        std::string ValueExpression(const SCompiledParameter& parameter)
        {
            std::string value = "mat.Values[" + std::to_string(parameter.Slot) + "]";

            switch (parameter.ValueType)
            {
                case EMaterialValueType::Float:    return value + ".x";
                case EMaterialValueType::Float2:   return value + ".xy";
                case EMaterialValueType::Float3:   return value + ".xyz";
                case EMaterialValueType::Float4:   return value;
            }

            return value;
        }

        std::string GenerateGraphHLSL(
            const MaterialGraph& graph,
            const SCompiledMaterial& material
        )
        {
            SMaterialGraphBindings bindings;

            for (const auto& parameter : material.Parameters)
            {
                const auto expression = parameter.Kind == EMaterialParameterKind::Texture
                    ? "mat.TextureIndices[" + std::to_string(parameter.Slot) + "]"
                    : ValueExpression(parameter);

                auto& destination = parameter.Kind == EMaterialParameterKind::Texture
                    ? bindings.Textures
                    : bindings.Values;

                destination[parameter.Name] = expression;
            }

            return graph.GenerateHLSL(bindings);
        }
    }

    SCompileResult Compiler::Build(const Material& material)
    {
        std::string diagnostics;
        if (!material.ValidateGraph(&diagnostics))
            return { .Diagnostics = std::move(diagnostics) };

        std::vector<std::pair<std::string, SMaterialParameterDefinition>> parameters(
            material.GetParameters().begin(),
            material.GetParameters().end()
        );
        std::ranges::sort(parameters, {}, &decltype(parameters)::value_type::first);

        SMaterialGraphBindings bindings;
        std::vector<SCompiledParameter> layout;
        uint32_t valueSlot = 0;
        uint32_t textureSlot = 0;

        for (const auto& [name, definition] : parameters)
        {
            auto& slot = definition.Kind == EMaterialParameterKind::Texture
                ? textureSlot
                : valueSlot;

            if (slot >= 32)
                return { .Diagnostics = "Material parameter capacity exceeded." };

            layout.push_back({ name, definition.Kind, definition.ValueType, slot });

            if (definition.Kind == EMaterialParameterKind::Texture)
                bindings.Textures[name] = "mat.TextureIndices[" + std::to_string(slot++) + "]";
            else
                bindings.Values[name] = "mat.Values[" + std::to_string(slot++) + "]";
        }

        const auto compiled = CreateRef<SCompiledMaterial>();
        compiled->Usage = material.GetUsage();
        compiled->MaterialRevision = material.GetRevision();
        compiled->Parameters = std::move(layout);
        return { .Material = compiled };
    }

    SCompileResult Compiler::Compile(const ShaderLoader* loader, const Material& material)
    {
        auto result = Build(material);
        if (!result) return result;

        if (material.GetUsage() == EMaterialUsage::Surface)
        {
            result = CompileSurface(loader, material, std::move(result));
            if (!result) return result;
        }

        if (material.GetUsage() == EMaterialUsage::Particle)
        {
            result = CompileParticleSprite(loader, material, std::move(result));
            if (!result) return result;
            result = CompileParticleRibbon(loader, material, std::move(result));
            if (!result) return result;
            result = CompileParticleMesh(loader, material, std::move(result));
            if (!result) return result;
        }

        return result;
    }

    std::string Compiler::InjectBody(
        const std::string& hlsl,
        const std::string& graphBody,
        const EMaterialShadingModel shadingModel
    )
    {
        std::string out = hlsl;

        constexpr std::string_view graphMarker = "// __GRAPH_BODY__";
        if (const auto pos = out.find(graphMarker); pos != std::string::npos)
            out.replace(pos, graphMarker.size(), graphBody);

        constexpr std::string_view shadingModelMarker = "// __SHADING_MODEL__";
        if (const auto pos = out.find(shadingModelMarker); pos != std::string::npos)
        {
            const auto value = (uint8_t)shadingModel;
            out.replace(
                pos,
                shadingModelMarker.size(),
                "#define MATERIAL_SHADING_MODEL " + std::to_string(value)
            );
        }

        return out;
    }

    SCompileResult Compiler::CompileSurface(
        const ShaderLoader* loader,
        const Material& material,
        SCompileResult result
    )
    {
        const auto vertexHlsl = ReadFile(s_ShadersDir / "Material" / "Surface.vs.hlsl");
        const auto pixelHlsl = ReadFile(s_ShadersDir / "Material" / "Material.ps.hlsl");

        if (vertexHlsl.empty() || pixelHlsl.empty())
        {
            EE_CORE_ERROR("Surface material shader template was not found.")
            result.Diagnostics = "Surface material shader template was not found.";
            result.Material.reset();
            return result;
        }

        // Unique name per compiled graph so instances don't clobber each other.
        static std::atomic<uint32_t> counter{ 0 };
        const std::string name = "GraphMat_" + std::to_string(counter.fetch_add(1));

        // Each compile loads from its own subdir containing only its two SPIR-V
        // modules, so stray files (like the generated.hlsl) never look like a
        // shader module to the loader. The .hlsl source lives outside that dir.
        const fs::path loadDir = s_GeneratedDir / name;
        std::error_code error;
        fs::create_directories(loadDir, error);

        const fs::path vertexSourcePath = s_GeneratedDir / (name + ".src.vs.hlsl");
        {
            std::ofstream out(vertexSourcePath, std::ios::binary);
            out << vertexHlsl;
        }

        const fs::path pixelSourcePath = s_GeneratedDir / (name + ".src.ps.hlsl");
        {
            std::ofstream out(pixelSourcePath, std::ios::binary);

            const auto graphHlsl = GenerateGraphHLSL(material.GetGraph(), *result.Material);
            out << InjectBody(
                pixelHlsl,
                graphHlsl,
                material.GetShadingModel()
            );
        }

        const fs::path dxc = FindDXC();

        const auto compileStage = [&dxc](
            const fs::path& sourcePath,
            const fs::path& spvPath,
            const std::string_view profile
        )
        {
            const std::string cmd =
                "\"" + dxc.string() + "\" -spirv -T " + std::string(profile) + " -E main \""
                + sourcePath.string() + "\" -Fo \"" + spvPath.string() + "\"";

            return std::system(cmd.c_str()) == 0 && fs::exists(spvPath);
        };

        if (!compileStage(
            vertexSourcePath,
            loadDir / (name + ".vs.spirv"),
            "vs_6_0"
        ) || !compileStage(
            pixelSourcePath,
            loadDir / (name + ".ps.spirv"),
            "ps_6_0"
        ))
        {
            EE_CORE_ERROR(
                "Surface material: DXC compilation failed for {}.",
                name
            )
            result.Diagnostics = "DXC failed while compiling the surface material.";
            result.Material.reset();
            return result;
        }

        result.Material->SurfaceShader = loader->LoadShader(loadDir, name);
        if (!result.Material->SurfaceShader)
        {
            result.Diagnostics = "Shader loader could not load the compiled surface material.";
            result.Material.reset();
        }

        return result;
    }

    SCompileResult Compiler::CompileParticleSprite(
        const ShaderLoader* loader,
        const Material& material,
        SCompileResult result
    )
    {
        const auto vertexHlsl = ReadFile(s_ShadersDir / "Material" / "ParticleSprite.vs.hlsl");
        const auto pixelHlsl = ReadFile(s_ShadersDir / "Material" / "ParticleSprite.ps.hlsl");

        if (vertexHlsl.empty() || pixelHlsl.empty())
        {
            result.Diagnostics = "Material sprite shader template was not found.";
            result.Material.reset();
            return result;
        }

        // Unique name per compiled graph so instances don't clobber each other.
        static std::atomic<uint32_t> counter{ 0 };
        const std::string name = "GraphMat_" + std::to_string(counter.fetch_add(1)) + "_ParticleSprite";

        const fs::path loadDir = s_GeneratedDir / name;
        std::error_code error;
        fs::create_directories(loadDir, error);

        const fs::path vertexSourcePath = s_GeneratedDir / (name + ".src.vs.hlsl");
        {
            std::ofstream out(vertexSourcePath, std::ios::binary);
            out << vertexHlsl;
        }

        const fs::path pixelSourcePath = s_GeneratedDir / (name + ".src.ps.hlsl");
        {
            std::ofstream out(pixelSourcePath, std::ios::binary);

            const auto graphHlsl = GenerateGraphHLSL(material.GetGraph(), *result.Material);
            out << InjectBody(pixelHlsl, graphHlsl);
        }

        const fs::path dxc = FindDXC();
        const auto compileStage = [&dxc](
            const fs::path& sourcePath,
            const fs::path& spvPath,
            const std::string_view profile
        )
        {
            const std::string cmd =
                "\"" + dxc.string() + "\" -spirv -T " + std::string(profile) + " -E main \""
                + sourcePath.string() + "\" -Fo \"" + spvPath.string() + "\"";

            return std::system(cmd.c_str()) == 0 && fs::exists(spvPath);
        };

        if (!compileStage(
            vertexSourcePath,
            loadDir / (name + ".vs.spirv"),
            "vs_6_0"
        ) || !compileStage(
            pixelSourcePath,
            loadDir / (name + ".ps.spirv"),
            "ps_6_0"
        ))
        {
            EE_CORE_ERROR(
                "Particle sprite material: DXC compilation failed for {}.",
                name
            )
            result.Diagnostics = "DXC failed while compiling the particle sprite material.";
            result.Material.reset();
            return result;
        }

        result.Material->ParticleSpriteShader = loader->LoadShader(loadDir, name);
        if (!result.Material->ParticleSpriteShader)
        {
            result.Diagnostics = "Shader loader could not load the particle sprite material.";
            result.Material.reset();
        }

        return result;
    }

    SCompileResult Compiler::CompileParticleRibbon(
        const ShaderLoader* loader,
        const Material& material,
        SCompileResult result
    )
    {
        const auto vertexHlsl = ReadFile(s_ShadersDir / "Material" / "ParticleRibbon.vs.hlsl");
        const auto pixelHlsl = ReadFile(s_ShadersDir / "Material" / "ParticleRibbon.ps.hlsl");

        if (vertexHlsl.empty() || pixelHlsl.empty())
        {
            result.Diagnostics = "Material ribbon shader template was not found.";
            result.Material.reset();
            return result;
        }

        // Unique name per compiled graph so instances don't clobber each other.
        static std::atomic<uint32_t> counter{ 0 };
        const std::string name = "GraphMat_" + std::to_string(counter.fetch_add(1)) + "_ParticleRibbon";

        const fs::path loadDir = s_GeneratedDir / name;
        std::error_code error;
        fs::create_directories(loadDir, error);

        const fs::path vertexSourcePath = s_GeneratedDir / (name + ".src.vs.hlsl");
        {
            std::ofstream out(vertexSourcePath, std::ios::binary);
            out << vertexHlsl;
        }

        const fs::path pixelSourcePath = s_GeneratedDir / (name + ".src.ps.hlsl");
        {
            std::ofstream out(pixelSourcePath, std::ios::binary);

            const auto graphHlsl = GenerateGraphHLSL(material.GetGraph(), *result.Material);
            out << InjectBody(pixelHlsl, graphHlsl);
        }

        // Compile the generated pixel shader to SPIR-V with DXC.
        const fs::path dxc = FindDXC();
        const fs::path spvPath = loadDir / (name + ".ps.spirv");

        const auto compileStage = [&dxc](
            const fs::path& sourcePath,
            const fs::path& spvPath,
            const std::string_view profile
        )
        {
            const std::string cmd =
                "\"" + dxc.string() + "\" -spirv -T " + std::string(profile) + " -E main \""
                + sourcePath.string() + "\" -Fo \"" + spvPath.string() + "\"";

            return std::system(cmd.c_str()) == 0 && fs::exists(spvPath);
        };

        if (!compileStage(
            vertexSourcePath,
            loadDir / (name + ".vs.spirv"),
            "vs_6_0"
        ) || !compileStage(
            pixelSourcePath,
            loadDir / (name + ".ps.spirv"),
            "ps_6_0"
        ))
        {
            EE_CORE_ERROR(
                "Particle ribbon material: DXC compilation failed for {}.",
                name
            )
            result.Diagnostics = "DXC failed while compiling the particle ribbon material.";
            result.Material.reset();
            return result;
        }

        result.Material->ParticleRibbonShader = loader->LoadShader(loadDir, name);
        if (!result.Material->ParticleRibbonShader)
        {
            result.Diagnostics = "Shader loader could not load the particle ribbon material.";
            result.Material.reset();
        }

        return result;
    }

    SCompileResult Compiler::CompileParticleMesh(
        const ShaderLoader* loader,
        const Material& material,
        SCompileResult result
    )
    {
        const auto vertexHlsl = ReadFile(s_ShadersDir / "Material" / "ParticleMesh.vs.hlsl");
        const auto pixelHlsl = ReadFile(s_ShadersDir / "Material" / "ParticleMesh.ps.hlsl");

        if (vertexHlsl.empty() || pixelHlsl.empty())
        {
            result.Diagnostics = "Material mesh shader template was not found.";
            result.Material.reset();
            return result;
        }

        // Unique name per compiled graph so instances don't clobber each other.
        static std::atomic<uint32_t> counter{ 0 };
        const std::string name = "GraphMat_" + std::to_string(counter.fetch_add(1)) + "_ParticleMesh";

        const fs::path loadDir = s_GeneratedDir / name;
        std::error_code error;
        fs::create_directories(loadDir, error);

        const fs::path vertexSourcePath = s_GeneratedDir / (name + ".src.vs.hlsl");
        {
            std::ofstream out(vertexSourcePath, std::ios::binary);
            out << vertexHlsl;
        }

        const fs::path pixelSourcePath = s_GeneratedDir / (name + ".src.ps.hlsl");
        {
            std::ofstream out(pixelSourcePath, std::ios::binary);

            const auto graphHlsl = GenerateGraphHLSL(material.GetGraph(), *result.Material);
            out << InjectBody(pixelHlsl, graphHlsl);
        }

        // Compile the generated pixel shader to SPIR-V with DXC.
        const fs::path dxc = FindDXC();
        const fs::path spvPath = loadDir / (name + ".ps.spirv");

        const auto compileStage = [&dxc](
            const fs::path& sourcePath,
            const fs::path& spvPath,
            const std::string_view profile
        )
        {
            const std::string cmd =
                "\"" + dxc.string() + "\" -spirv -T " + std::string(profile) + " -E main \""
                + sourcePath.string() + "\" -Fo \"" + spvPath.string() + "\"";

            return std::system(cmd.c_str()) == 0 && fs::exists(spvPath);
        };

        if (!compileStage(
            vertexSourcePath,
            loadDir / (name + ".vs.spirv"),
            "vs_6_0"
        ) || !compileStage(
            pixelSourcePath,
            loadDir / (name + ".ps.spirv"),
            "ps_6_0"
        ))
        {
            EE_CORE_ERROR(
                "Particle mesh material: DXC compilation failed for {}.",
                name
            )
            result.Diagnostics = "DXC failed while compiling the particle mesh material.";
            result.Material.reset();
            return result;
        }

        result.Material->ParticleMeshShader = loader->LoadShader(loadDir, name);
        if (!result.Material->ParticleMeshShader)
        {
            result.Diagnostics = "Shader loader could not load the particle mesh material.";
            result.Material.reset();
        }

        return result;
    }
}
