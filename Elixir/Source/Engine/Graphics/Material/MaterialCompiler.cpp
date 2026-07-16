#include "epch.h"
#include "MaterialCompiler.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <atomic>

namespace Elixir
{
    namespace fs = std::filesystem;

    namespace
    {
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

    Ref<Shader> MaterialCompiler::Compile(const ShaderLoader* loader, const MaterialGraph& graph)
    {
        const auto compiled = CompileToSpirv(graph);
        if (!compiled)
            return nullptr;
        return LoadCompiled(loader, *compiled);
    }

    Ref<Shader> MaterialCompiler::LoadCompiled(const ShaderLoader* loader, const SCompiled& compiled)
    {
        return loader->LoadShader(compiled.LoadDir, compiled.Name);
    }

    std::optional<MaterialCompiler::SCompiled> MaterialCompiler::CompileToSpirv(const MaterialGraph& graph)
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

        const fs::path shadersDir = "./Shaders";
        const fs::path generatedDir = shadersDir / "Generated";

        const std::string templateHlsl = ReadFile(shadersDir / "GraphMaterial.ps.hlsl");
        if (templateHlsl.empty())
        {
            EE_CORE_ERROR("Material graph: template GraphMaterial.ps.hlsl not found next to the shaders.")
            return std::nullopt;
        }

        // Unique name per compiled graph so instances don't clobber each other.
        static std::atomic<uint32_t> counter{ 0 };
        const std::string name = "GraphMat_" + std::to_string(counter.fetch_add(1));

        // Each compile loads from its own subdir containing only its two SPIR-V
        // modules, so stray files (like the generated .hlsl) never look like a
        // shader module to the loader. The .hlsl source lives outside that dir.
        const fs::path loadDir = generatedDir / name;
        std::error_code ec;
        fs::create_directories(loadDir, ec);

        const fs::path hlslPath = generatedDir / (name + ".src.ps.hlsl");
        {
            std::ofstream out(hlslPath, std::ios::binary);
            out << InjectBody(templateHlsl, graph);
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
        if (!compileStage(InjectBody(templateHlsl, graph), "ps_6_0", hlslPath, loadDir / (name + ".ps.spirv")))
        {
            EE_CORE_ERROR("Material graph: DXC pixel-shader compilation failed for {0}.", name)
            return std::nullopt;
        }

        // Vertex stage: World Position Offset (displaces the vertex; no-op if the
        // graph doesn't drive that channel).
        const std::string vsTemplate = ReadFile(shadersDir / "GraphMaterial.vs.hlsl");
        if (vsTemplate.empty())
        {
            EE_CORE_ERROR("Material graph: template GraphMaterial.vs.hlsl not found next to the shaders.")
            return std::nullopt;
        }
        std::string vsSource = vsTemplate;
        if (const auto pos = vsSource.find("// __WPO_BODY__"); pos != std::string::npos)
            vsSource.replace(pos, std::strlen("// __WPO_BODY__"), graph.GenerateHLSL(true));
        if (!compileStage(vsSource, "vs_6_0", generatedDir / (name + ".src.vs.hlsl"), loadDir / (name + ".vs.spirv")))
        {
            EE_CORE_ERROR("Material graph: DXC vertex-shader compilation failed for {0}.", name)
            return std::nullopt;
        }

        return SCompiled{ loadDir, name };
    }
}
