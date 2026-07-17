#include "epch.h"
#include "MaterialCompiler.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <iomanip>

namespace Elixir
{
    namespace fs = std::filesystem;

    namespace
    {
        constexpr uint32_t SHADER_FORMAT_VERSION = 3;
        constexpr std::string_view SHADER_PLATFORM = "SPIRV.ps_6_0.vs_6_0";

        fs::path FindDxc()
        {
            if (const char* sdk = std::getenv("VULKAN_SDK"))
            {
                const fs::path mac = fs::path(sdk) / "macOS" / "bin" / "dxc";
                if (fs::exists(mac)) return mac;
                const fs::path bin = fs::path(sdk) / "bin" / "dxc";
                if (fs::exists(bin)) return bin;
            }
            return "dxc"; // rely on PATH
        }

        std::string ReadFile(const fs::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};
            std::stringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        std::string BuildVertexSource(const std::string_view vertexTemplate, const MaterialGraph& graph)
        {
            std::string source(vertexTemplate);
            if (const auto pos = source.find("// __WPO_BODY__"); pos != std::string::npos)
                source.replace(pos, std::strlen("// __WPO_BODY__"), graph.GenerateHLSL(true));
            return source;
        }

        uint64_t HashSources(const std::string_view pixelSource, const std::string_view vertexSource,
            const SMaterialShaderPermutation& permutation)
        {
            uint64_t hash = 1469598103934665603ull;
            const auto mix = [&](const std::string_view bytes)
            {
                for (const unsigned char byte : bytes)
                {
                    hash ^= byte;
                    hash *= 1099511628211ull;
                }
                // Separate fields so concatenated strings cannot alias one another.
                hash ^= 0xffu;
                hash *= 1099511628211ull;
            };
            mix(SHADER_PLATFORM);
            const std::string version = std::to_string(SHADER_FORMAT_VERSION);
            mix(version);
            const std::string domainValue = std::to_string((uint32_t)permutation.Domain);
            const std::string usageValue = std::to_string((uint32_t)permutation.Usage);
            const std::string vertexFactoryValue = std::to_string((uint32_t)permutation.VertexFactory);
            const std::string passValue = std::to_string((uint32_t)permutation.Pass);
            mix(domainValue);
            mix(usageValue);
            mix(vertexFactoryValue);
            mix(passValue);
            mix(pixelSource);
            mix(vertexSource);
            return hash;
        }

        bool IsCompleteArtifact(const fs::path& path)
        {
            std::error_code ec;
            return fs::is_regular_file(path, ec) && fs::file_size(path, ec) > 0 && !ec;
        }
    }

    std::string MaterialCompiler::InjectBody(const std::string& templateHlsl, const MaterialGraph& graph)
    {
        std::string out = templateHlsl;
        const std::string marker = "// __GRAPH_BODY__";
        if (const auto pos = out.find(marker); pos != std::string::npos)
            out.replace(pos, marker.size(), graph.GenerateHLSL());

        // Set the shading model (a compile-time #define the shading branch reads).
        const std::string smMarker = "// __SHADING_MODEL__";
        if (const auto pos = out.find(smMarker); pos != std::string::npos)
            out.replace(pos, smMarker.size(),
                "#define SHADING_MODEL " + std::to_string((int)graph.GetShadingModel()) + "u");
        return out;
    }

    uint64_t MaterialCompiler::BuildContentKeyFromTemplates(const MaterialGraph& graph,
        const SMaterialShaderPermutation& permutation,
        const std::string_view pixelTemplate, const std::string_view vertexTemplate)
    {
        const std::string pixelSource = InjectBody(std::string(pixelTemplate), graph);
        const std::string vertexSource = BuildVertexSource(vertexTemplate, graph);
        return HashSources(pixelSource, vertexSource, permutation);
    }

    std::optional<uint64_t> MaterialCompiler::BuildContentKey(const MaterialGraph& graph,
        const SMaterialShaderPermutation& permutation)
    {
        if (!MaterialShaderPermutation::Matches(
                graph.GetDomain(), graph.GetUsages(), permutation)
            || !MaterialShaderPermutation::IsSupported(permutation))
            return std::nullopt;

        const SMaterialDomainDescriptor* domain = MaterialDomain::Find(graph.GetDomain());
        if (!domain || !domain->ShaderContractAvailable
            || domain->PixelTemplate.empty() || domain->VertexTemplate.empty())
            return std::nullopt;

        const fs::path shadersDir = "./Shaders";
        const std::string pixelTemplate = ReadFile(shadersDir / domain->PixelTemplate);
        const std::string vertexTemplate = ReadFile(shadersDir / domain->VertexTemplate);
        if (pixelTemplate.empty() || vertexTemplate.empty())
            return std::nullopt;
        return BuildContentKeyFromTemplates(graph, permutation, pixelTemplate, vertexTemplate);
    }

    std::string MaterialCompiler::CacheName(const uint64_t contentKey)
    {
        std::ostringstream name;
        name << "GraphMat_" << std::hex << std::setw(16) << std::setfill('0') << contentKey;
        return name.str();
    }

    Ref<Shader> MaterialCompiler::Compile(const ShaderLoader* loader, const MaterialGraph& graph,
        const SMaterialShaderPermutation& permutation)
    {
        const auto compiled = CompileToSpirv(graph, permutation);
        if (!compiled)
            return nullptr;
        return LoadCompiled(loader, *compiled);
    }

    Ref<Shader> MaterialCompiler::LoadCompiled(const ShaderLoader* loader, const SCompiled& compiled)
    {
        return loader->LoadShader(compiled.LoadDir, compiled.Name);
    }

    std::optional<MaterialCompiler::SCompiled> MaterialCompiler::CompileToSpirv(
        const MaterialGraph& graph, const SMaterialShaderPermutation& permutation)
    {
        const SMaterialGraphValidation validation = graph.Validate();
        for (const auto& diagnostic : validation.Diagnostics)
        {
            if (diagnostic.Severity == EMaterialDiagnosticSeverity::Error)
                EE_CORE_ERROR("Material graph validation (node {}): {}", diagnostic.NodeId, diagnostic.Message)
            else
                EE_CORE_WARN("Material graph validation (node {}): {}", diagnostic.NodeId, diagnostic.Message)
        }
        if (validation.HasErrors())
            return std::nullopt;

        if (!MaterialShaderPermutation::Matches(
                graph.GetDomain(), graph.GetUsages(), permutation))
        {
            EE_CORE_ERROR("Material graph: requested shader permutation is not declared by the material.")
            return std::nullopt;
        }
        if (!MaterialShaderPermutation::IsSupported(permutation))
        {
            EE_CORE_ERROR("Material graph: requested shader permutation has no renderer contract yet.")
            return std::nullopt;
        }

        const SMaterialDomainDescriptor* domain = MaterialDomain::Find(graph.GetDomain());
        if (!domain || !domain->ShaderContractAvailable
            || domain->PixelTemplate.empty() || domain->VertexTemplate.empty())
        {
            EE_CORE_ERROR("Material graph: selected domain has no renderer shader contract.")
            return std::nullopt;
        }

        const fs::path shadersDir = "./Shaders";
        const fs::path generatedDir = shadersDir / "Generated";

        const std::string templateHlsl = ReadFile(shadersDir / domain->PixelTemplate);
        if (templateHlsl.empty())
        {
            EE_CORE_ERROR("Material graph: template '{}' not found next to the shaders.", domain->PixelTemplate)
            return std::nullopt;
        }

        const std::string vsTemplate = ReadFile(shadersDir / domain->VertexTemplate);
        if (vsTemplate.empty())
        {
            EE_CORE_ERROR("Material graph: template '{}' not found next to the shaders.", domain->VertexTemplate)
            return std::nullopt;
        }

        const std::string pixelSource = InjectBody(templateHlsl, graph);
        const std::string vertexSource = BuildVertexSource(vsTemplate, graph);
        const uint64_t contentKey = HashSources(pixelSource, vertexSource, permutation);
        const std::string name = CacheName(contentKey);

        // Each compile loads from its own subdir containing only its two SPIR-V
        // modules, so stray files (like the generated .hlsl) never look like a
        // shader module to the loader. The .hlsl source lives outside that dir.
        const fs::path loadDir = generatedDir / name;
        std::error_code ec;
        fs::create_directories(loadDir, ec);

        const fs::path hlslPath = generatedDir / (name + ".src.ps.hlsl");
        const fs::path vsHlslPath = generatedDir / (name + ".src.vs.hlsl");
        const fs::path pixelSpirv = loadDir / (name + ".ps.spirv");
        const fs::path vertexSpirv = loadDir / (name + ".vs.spirv");
        if (IsCompleteArtifact(pixelSpirv) && IsCompleteArtifact(vertexSpirv))
        {
            EE_CORE_TRACE("Material shader cache hit: {}.", name)
            return SCompiled{ loadDir, name, permutation };
        }

        const fs::path dxc = FindDxc();

        // Compiles one HLSL string to SPIR-V; returns false on failure.
        const auto compileStage = [&](const std::string& hlsl, const char* profile, const fs::path& src, const fs::path& out)
        {
            { std::ofstream f(src, std::ios::binary); f << hlsl; }
            const std::string cmd = "\"" + dxc.string() + "\" -spirv -T " + profile
                + " -E main \"" + src.string() + "\" -Fo \"" + out.string() + "\"";
            return std::system(cmd.c_str()) == 0 && fs::exists(out);
        };

        // Pixel stage: surface channels.
        if (!compileStage(pixelSource, "ps_6_0", hlslPath, pixelSpirv))
        {
            EE_CORE_ERROR("Material graph: DXC pixel-shader compilation failed for {0}.", name)
            return std::nullopt;
        }

        // Vertex stage: World Position Offset (displaces the vertex; no-op if the
        // graph doesn't drive that channel).
        if (!compileStage(vertexSource, "vs_6_0", vsHlslPath, vertexSpirv))
        {
            EE_CORE_ERROR("Material graph: DXC vertex-shader compilation failed for {0}.", name)
            return std::nullopt;
        }

        return SCompiled{ loadDir, name, permutation };
    }
}
