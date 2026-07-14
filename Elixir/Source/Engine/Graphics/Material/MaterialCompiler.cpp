#include "epch.h"
#include "MaterialCompiler.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
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
        return out;
    }

    Ref<Shader> MaterialCompiler::Compile(const ShaderLoader* loader, const MaterialGraph& graph)
    {
        const fs::path shadersDir = "./Shaders";
        const fs::path generatedDir = shadersDir / "Generated";

        const std::string templateHlsl = ReadFile(shadersDir / "GraphMaterial.ps.hlsl");
        if (templateHlsl.empty())
        {
            EE_CORE_ERROR("Material graph: template GraphMaterial.ps.hlsl not found next to the shaders.")
            return nullptr;
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

        // Compile the generated pixel shader to SPIR-V with DXC.
        const fs::path dxc = FindDxc();
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

        // Reuse the shared model vertex shader (graph materials only vary the pixel
        // stage). LoadShader wants <name>.vs.spirv and <name>.ps.spirv side by side.
        fs::copy_file(shadersDir / "Model.vs.spirv", loadDir / (name + ".vs.spirv"),
            fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            EE_CORE_ERROR("Material graph: could not stage Model.vs.spirv ({0}).", ec.message())
            return nullptr;
        }

        return loader->LoadShader(loadDir, name);
    }
}
