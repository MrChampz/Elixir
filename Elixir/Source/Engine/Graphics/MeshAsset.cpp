#include "epch.h"
#include "MeshAsset.h"

#include <simdjson.h>

#include <algorithm>
#include <fstream>

namespace Elixir
{
    namespace fs = std::filesystem;

    namespace
    {
        fs::path AbsoluteNormalized(const fs::path& path)
        {
            std::error_code ec;
            const fs::path absolute = fs::absolute(path, ec);
            return (ec ? path : absolute).lexically_normal();
        }

        std::string MakeReference(const fs::path& assetPath, const fs::path& targetPath)
        {
            std::error_code ec;
            const fs::path relative = fs::relative(
                AbsoluteNormalized(targetPath), AbsoluteNormalized(assetPath.parent_path()), ec);
            return (ec ? targetPath : relative).generic_string();
        }

        fs::path ResolveReference(const fs::path& assetPath, const std::string_view reference)
        {
            const fs::path path(reference);
            return path.is_absolute() ? path.lexically_normal()
                                     : (assetPath.parent_path() / path).lexically_normal();
        }

        void WriteString(std::ostream& out, const std::string_view value)
        {
            out << '"';
            for (const char c : value)
            {
                if (c == '"') out << "\\\"";
                else if (c == '\\') out << "\\\\";
                else out << c;
            }
            out << '"';
        }
    }

    bool MeshAsset::Save(const fs::path& path, const SMeshAssetDescription& description)
    {
        if (description.Source.empty())
            return false;
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream out(path);
        if (!out)
        {
            EE_CORE_ERROR("Mesh asset: failed to open '{}' for writing.", path.string())
            return false;
        }

        std::vector<SMeshMaterialSlot> materials = description.Materials;
        std::sort(materials.begin(), materials.end(), [](const auto& a, const auto& b) { return a.Slot < b.Slot; });
        out << "{\n  \"version\": 1,\n  \"assetType\": \"Mesh\",\n  \"source\": ";
        WriteString(out, MakeReference(path, description.Source));
        out << ",\n  \"materials\": [\n";
        for (size_t i = 0; i < materials.size(); ++i)
        {
            out << "    { \"slot\": " << materials[i].Slot << ", \"asset\": ";
            WriteString(out, MakeReference(path, materials[i].Material));
            out << " }" << (i + 1 < materials.size() ? "," : "") << '\n';
        }
        out << "  ]\n}\n";
        EE_CORE_INFO("Mesh asset saved to '{}'.", path.string())
        return true;
    }

    std::optional<SMeshAssetDescription> MeshAsset::Load(const fs::path& path)
    {
        auto json = simdjson::padded_string::load(path.string());
        if (json.error())
        {
            EE_CORE_ERROR("Mesh asset: failed to open '{}': {}", path.string(),
                simdjson::error_message(json.error()))
            return std::nullopt;
        }
        simdjson::dom::parser parser;
        simdjson::dom::element document;
        if (const auto error = parser.parse(json.value()).get(document))
        {
            EE_CORE_ERROR("Mesh asset: failed to parse '{}': {}", path.string(),
                simdjson::error_message(error))
            return std::nullopt;
        }

        int64_t version;
        std::string_view source;
        if (document["version"].get(version) != simdjson::SUCCESS || version != 1
            || document["source"].get(source) != simdjson::SUCCESS)
        {
            EE_CORE_ERROR("Mesh asset: '{}' has an invalid header.", path.string())
            return std::nullopt;
        }

        SMeshAssetDescription description;
        description.Source = ResolveReference(path, source);
        simdjson::dom::array materials;
        if (document["materials"].get(materials) != simdjson::SUCCESS)
            return std::nullopt;
        for (const auto element : materials)
        {
            uint64_t slot;
            std::string_view material;
            if (element["slot"].get(slot) != simdjson::SUCCESS
                || slot > std::numeric_limits<uint32_t>::max()
                || element["asset"].get(material) != simdjson::SUCCESS)
                return std::nullopt;
            description.Materials.push_back({ (uint32_t)slot, ResolveReference(path, material) });
        }
        return description;
    }
}
