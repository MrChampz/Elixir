#include "epch.h"
#include "MaterialCompiler.h"

namespace Elixir
{
    namespace fs = std::filesystem;

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
    }

    Ref<Shader> MaterialCompiler::Compile(const ShaderLoader* loader, const MaterialGraph& graph)
    {
        const fs::path shadersDir = "./Shaders";
        const fs::path generatedDir = shadersDir / "Generated";

        const auto hlsl = ReadFile(shadersDir / "Material" / "Material.ps.hlsl");

        if (hlsl.empty())
        {
            EE_CORE_ERROR("Material graph: template Material.ps.hlsl not found.")
            return nullptr;
        }

        // Unique name per compiled graph so instances don't clobber each other.
        static std::atomic<uint32_t> counter{ 0 };
        const std::string name = "GraphMat_" + std::to_string(counter.fetch_add(1));

        // Each compile loads from its own subdir containing only its two SPIR-V
        // modules, so stray files (like the generated.hlsl) never look like a
        // shader module to the loader. The .hlsl source lives outside that dir.
        const fs::path loadDir = generatedDir / name;
        std::error_code error;
        fs::create_directories(loadDir, error);

        const fs::path hlslPath = generatedDir / (name + ".src.ps.hlsl");
        {
            std::ofstream out(hlslPath, std::ios::binary);
            out << InjectBody(hlsl, graph);
        }

        // Compile the generated pixel shader to SPIR-V with DXC.
        const fs::path dxc = FindDXC();
        const fs::path spvPath = loadDir / (name + ".ps.spirv");
        const std::string cmd =
            "\"" + dxc.string() + "\" -spirv -T ps_6_0 -E main \""
            + hlslPath.string() + "\" -Fo \"" + spvPath.string() + "\"";

        const int rc = std::system(cmd.c_str());
        if (rc != 0 || !fs::exists(spvPath))
        {
            EE_CORE_ERROR("Material graph: DXC compilation failed (rc={0}) for {1}.", rc, name)
            return nullptr;
        }

        return loader->LoadShader(loadDir, name);
    }

    std::string MaterialCompiler::InjectBody(const std::string& hlsl, const MaterialGraph& graph)
    {
        std::string out = hlsl;

        constexpr std::string marker = "// __GRAPH_BODY__";
        if (const auto pos = out.find(marker); pos != std::string::npos)
            out.replace(pos, marker.size(), graph.GenerateHLSL());

        return out;
    }
}
