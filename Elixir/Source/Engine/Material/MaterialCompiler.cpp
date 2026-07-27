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

        std::string ValueExpression(const SCompiledMaterialParameter& parameter)
        {
            const std::string value = "mat.Values[" + std::to_string(parameter.Slot) + "]";

            switch (parameter.ValueType)
            {
                case EMaterialGraphValueType::Float:    return value + ".x";
                case EMaterialGraphValueType::Float2:   return value + ".xy";
                case EMaterialGraphValueType::Float3:   return value + ".xyz";
                case EMaterialGraphValueType::Float4:   return value;
            }

            return value;
        }
    }

    SMaterialCompileResult MaterialCompiler::Build(const Material& material)
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
        std::vector<SCompiledMaterialParameter> layout;
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
        compiled->UsageMask = material.GetUsageMask();
        compiled->MaterialRevision = material.GetRevision();
        compiled->Parameters = std::move(layout);
        return { .Material = compiled };
    }

    SMaterialCompileResult MaterialCompiler::Compile(
        const ShaderLoader* loader,
        const Material& material
    )
    {
        auto result = Build(material);
        if (!result) return result;

        const fs::path shadersDir = "./Shaders";
        const fs::path generatedDir = shadersDir / "Generated";

        const auto hlsl = ReadFile(shadersDir / "Material" / "Material.ps.hlsl");

        if (hlsl.empty())
        {
            EE_CORE_ERROR("Material graph: template Material.ps.hlsl not found.")
            result.Diagnostics = "Material template Material.ps.hlsl was not found.";
            result.Material.reset();
            return result;
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

            SMaterialGraphBindings bindings;
            for (const auto& parameter : result.Material->Parameters)
            {
                const auto expr = parameter.Kind == EMaterialParameterKind::Texture
                    ? "mat.TextureIndices[" + std::to_string(parameter.Slot) + "]"
                    : ValueExpression(parameter);

                auto& binding = parameter.Kind == EMaterialParameterKind::Texture
                    ? bindings.Textures
                    : bindings.Values;

                binding[parameter.Name] = expr;
            }

            const auto graphHlsl = material.GetGraph().GenerateHLSL(bindings);
            out << InjectBody(hlsl, graphHlsl);
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
            result.Diagnostics = "DXC failed while compiling material.";
            result.Material.reset();
            return result;
        }

        result.Material->Shader = loader->LoadShader(loadDir, name);
        if (!result.Material->Shader)
        {
            result.Diagnostics = "Shader loader could not load the compiled material.";
            result.Material.reset();
        }

        return result;
    }

    std::string MaterialCompiler::InjectBody(
        const std::string& hlsl,
        const std::string& graphBody
    )
    {
        std::string out = hlsl;

        constexpr std::string marker = "// __GRAPH_BODY__";
        if (const auto pos = out.find(marker); pos != std::string::npos)
            out.replace(pos, marker.size(), graphBody);

        return out;
    }
}
