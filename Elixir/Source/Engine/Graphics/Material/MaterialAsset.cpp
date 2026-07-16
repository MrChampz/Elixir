#include "epch.h"
#include "MaterialAsset.h"

#include "MaterialGraphEditor.h"
#include <Engine/Graphics/TextureLoader.h>

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

        fs::path ResolveReference(const fs::path& assetPath, const std::string_view reference)
        {
            const fs::path path(reference);
            return path.is_absolute() ? path.lexically_normal()
                                     : (assetPath.parent_path() / path).lexically_normal();
        }

        std::string MakeReference(const fs::path& assetPath, const fs::path& targetPath)
        {
            if (targetPath.empty())
                return {};
            std::error_code ec;
            const fs::path relative = fs::relative(
                AbsoluteNormalized(targetPath), AbsoluteNormalized(assetPath.parent_path()), ec);
            return (ec ? targetPath : relative).generic_string();
        }

        void WriteString(std::ostream& out, const std::string_view value)
        {
            out << '"';
            for (const char c : value)
            {
                switch (c)
                {
                    case '"': out << "\\\""; break;
                    case '\\': out << "\\\\"; break;
                    case '\n': out << "\\n"; break;
                    case '\r': out << "\\r"; break;
                    case '\t': out << "\\t"; break;
                    default: out << c; break;
                }
            }
            out << '"';
        }

        bool OpenOutput(const fs::path& path, std::ofstream& out)
        {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            out.open(path);
            if (!out)
            {
                EE_CORE_ERROR("Material asset: failed to open '{}' for writing.", path.string())
                return false;
            }
            return true;
        }

        std::vector<std::pair<std::string, SMaterialParam>> SortedParameters(
            const std::unordered_map<std::string, SMaterialParam>& parameters)
        {
            std::vector<std::pair<std::string, SMaterialParam>> sorted(parameters.begin(), parameters.end());
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            return sorted;
        }

        bool ParametersCanBeSaved(const fs::path& assetPath,
            const std::unordered_map<std::string, SMaterialParam>& parameters)
        {
            for (const auto& [name, parameter] : parameters)
            {
                if (parameter.Type == SMaterialParam::EType::Texture
                    && parameter.TextureRef && parameter.TextureRef->GetPath().empty())
                {
                    EE_CORE_ERROR(
                        "Material asset: '{}' cannot save texture parameter '{}' because the texture has no source path.",
                        assetPath.string(), name)
                    return false;
                }
            }
            return true;
        }

        void WriteParameter(std::ostream& out, const fs::path& assetPath,
            const std::string& name, const SMaterialParam& parameter)
        {
            out << "{ \"name\": ";
            WriteString(out, name);
            out << ", \"type\": " << (int)parameter.Type << ", \"value\": ";
            switch (parameter.Type)
            {
                case SMaterialParam::EType::Scalar:
                    out << parameter.Scalar;
                    break;
                case SMaterialParam::EType::Vector:
                    out << '[' << parameter.Vector.x << ", " << parameter.Vector.y << ", "
                        << parameter.Vector.z << ", " << parameter.Vector.w << ']';
                    break;
                case SMaterialParam::EType::Texture:
                    if (parameter.TextureRef && !parameter.TextureRef->GetPath().empty())
                    {
                        out << "{ \"path\": ";
                        WriteString(out, MakeReference(assetPath, parameter.TextureRef->GetPath()));
                        out << ", \"format\": " << (int)parameter.TextureRef->GetFormat() << " }";
                    }
                    else
                        out << "null";
                    break;
            }
            out << " }";
        }

        std::optional<double> Number(const simdjson::dom::element element)
        {
            double value;
            if (element.get(value) == simdjson::SUCCESS)
                return value;
            int64_t integer;
            if (element.get(integer) == simdjson::SUCCESS)
                return (double)integer;
            return std::nullopt;
        }

        std::optional<std::pair<std::string, SMaterialParam>> ParseParameter(
            const simdjson::dom::element element, const fs::path& assetPath)
        {
            std::string_view name;
            int64_t rawType;
            if (element["name"].get(name) != simdjson::SUCCESS
                || element["type"].get(rawType) != simdjson::SUCCESS
                || rawType < (int64_t)SMaterialParam::EType::Scalar
                || rawType > (int64_t)SMaterialParam::EType::Texture)
                return std::nullopt;

            const auto type = (SMaterialParam::EType)rawType;
            const simdjson::dom::element value = element["value"];
            if (type == SMaterialParam::EType::Scalar)
            {
                const auto scalar = Number(value);
                if (!scalar)
                    return std::nullopt;
                return std::pair{ std::string(name), SMaterialParam::MakeScalar((float)*scalar) };
            }

            if (type == SMaterialParam::EType::Vector)
            {
                simdjson::dom::array array;
                if (value.get(array) != simdjson::SUCCESS)
                    return std::nullopt;
                glm::vec4 vector(0.0f);
                int index = 0;
                for (const auto component : array)
                {
                    const auto number = Number(component);
                    if (!number || index >= 4)
                        return std::nullopt;
                    (&vector.x)[index] = (float)*number;
                    ++index;
                }
                if (index != 4)
                    return std::nullopt;
                return std::pair{ std::string(name), SMaterialParam::MakeVector(vector) };
            }

            if (value.is_null())
                return std::pair{ std::string(name), SMaterialParam::MakeTexture(nullptr) };

            std::string_view textureReference;
            int64_t rawFormat = (int64_t)EImageFormat::R8G8B8A8_SRGB;
            if (value["path"].get(textureReference) != simdjson::SUCCESS)
                return std::nullopt;
            if (value["format"].get(rawFormat) != simdjson::SUCCESS)
                rawFormat = (int64_t)EImageFormat::R8G8B8A8_SRGB;
            const fs::path texturePath = ResolveReference(assetPath, textureReference);
            Ref<Texture> texture;
            if (fs::exists(texturePath))
                texture = TextureLoader::Load(texturePath, (EImageFormat)rawFormat);
            else
                EE_CORE_WARN("Material asset: texture '{}' does not exist.", texturePath.string())
            return std::pair{ std::string(name), SMaterialParam::MakeTexture(texture) };
        }

        bool LoadDocument(const fs::path& path, simdjson::padded_string& json,
            simdjson::dom::parser& parser, simdjson::dom::element& document)
        {
            auto loaded = simdjson::padded_string::load(path.string());
            if (loaded.error())
            {
                EE_CORE_ERROR("Material asset: failed to open '{}': {}", path.string(),
                    simdjson::error_message(loaded.error()))
                return false;
            }
            json = std::move(loaded).value();
            if (const auto error = parser.parse(json).get(document))
            {
                EE_CORE_ERROR("Material asset: failed to parse '{}': {}", path.string(),
                    simdjson::error_message(error))
                return false;
            }
            return true;
        }

        bool ReadVersion(const simdjson::dom::element document, const fs::path& path)
        {
            int64_t version;
            if (document["version"].get(version) != simdjson::SUCCESS || version != 1)
            {
                EE_CORE_ERROR("Material asset: '{}' has an unsupported version.", path.string())
                return false;
            }
            return true;
        }
    }

    bool MaterialAsset::SaveMaterial(
        const fs::path& path, const Material& material, const MaterialGraphEditor& editor)
    {
        if (!ParametersCanBeSaved(path, material.GetDefaults()))
            return false;
        std::ofstream out;
        if (!OpenOutput(path, out))
            return false;

        out << "{\n  \"version\": 1,\n  \"assetType\": \"Material\",\n  \"name\": ";
        WriteString(out, material.GetName());
        out << ",\n  \"defaults\": [\n";
        const auto defaults = SortedParameters(material.GetDefaults());
        for (size_t i = 0; i < defaults.size(); ++i)
        {
            out << "    ";
            WriteParameter(out, path, defaults[i].first, defaults[i].second);
            out << (i + 1 < defaults.size() ? "," : "") << '\n';
        }
        out << "  ],\n";
        editor.WriteAssetFields(out);
        out << "\n}\n";
        EE_CORE_INFO("Material asset saved to '{}'.", path.string())
        return true;
    }

    Ref<Material> MaterialAsset::LoadMaterial(const fs::path& path, MaterialGraphEditor* editor)
    {
        simdjson::padded_string json;
        simdjson::dom::parser parser;
        simdjson::dom::element document;
        if (!LoadDocument(path, json, parser, document) || !ReadVersion(document, path))
            return nullptr;

        std::string_view name;
        if (document["name"].get(name) != simdjson::SUCCESS)
        {
            EE_CORE_ERROR("Material asset: '{}' has no valid name.", path.string())
            return nullptr;
        }

        MaterialGraphEditor loadedEditor;
        if (!loadedEditor.Load(path.string()))
            return nullptr;
        const Ref<Material> graphMaterial = loadedEditor.BuildMaterial();

        auto material = CreateRef<Material>(std::string(name));
        simdjson::dom::array defaults;
        if (document["defaults"].get(defaults) != simdjson::SUCCESS)
            return nullptr;
        for (const auto element : defaults)
            if (auto parameter = ParseParameter(element, path))
                material->SetDefault(parameter->first, parameter->second);

        for (const auto& [parameterName, value] : graphMaterial->GetDefaults())
            if (!material->GetDefault(parameterName))
                material->SetDefault(parameterName, value);
        material->SetGraph(*graphMaterial->GetGraph(), graphMaterial->GetGraphParameters());
        if (editor)
            *editor = std::move(loadedEditor);

        EE_CORE_INFO("Material asset loaded from '{}'.", path.string())
        return material;
    }

    bool MaterialAsset::SaveInstance(const fs::path& path, const MaterialInstance& instance,
        const fs::path& parentPath)
    {
        if (!instance.GetParent() || parentPath.empty())
            return false;
        if (!ParametersCanBeSaved(path, instance.GetOverrides()))
            return false;

        std::ofstream out;
        if (!OpenOutput(path, out))
            return false;
        out << "{\n  \"version\": 1,\n  \"assetType\": \"MaterialInstance\",\n  \"name\": ";
        WriteString(out, instance.GetName());
        out << ",\n  \"parent\": ";
        WriteString(out, MakeReference(path, parentPath));
        out << ",\n  \"overrides\": [\n";
        const auto overrides = SortedParameters(instance.GetOverrides());
        for (size_t i = 0; i < overrides.size(); ++i)
        {
            out << "    ";
            WriteParameter(out, path, overrides[i].first, overrides[i].second);
            out << (i + 1 < overrides.size() ? "," : "") << '\n';
        }
        out << "  ]\n}\n";
        EE_CORE_INFO("Material instance saved to '{}'.", path.string())
        return true;
    }

    Ref<MaterialInstance> MaterialAsset::LoadInstance(
        const fs::path& path, MaterialGraphEditor* parentEditor)
    {
        simdjson::padded_string json;
        simdjson::dom::parser parser;
        simdjson::dom::element document;
        if (!LoadDocument(path, json, parser, document) || !ReadVersion(document, path))
            return nullptr;

        std::string_view parentReference;
        if (document["parent"].get(parentReference) != simdjson::SUCCESS)
            return nullptr;
        const Ref<Material> parent = LoadMaterial(ResolveReference(path, parentReference), parentEditor);
        if (!parent)
            return nullptr;

        auto instance = CreateRef<MaterialInstance>(parent);
        std::string_view name;
        if (document["name"].get(name) == simdjson::SUCCESS)
            instance->SetName(std::string(name));

        simdjson::dom::array overrides;
        if (document["overrides"].get(overrides) != simdjson::SUCCESS)
            return nullptr;
        for (const auto element : overrides)
        {
            const auto parameter = ParseParameter(element, path);
            if (!parameter)
                continue;
            const SMaterialParam* definition = parent->GetDefault(parameter->first);
            if (!definition || definition->Type != parameter->second.Type)
            {
                EE_CORE_WARN("Material instance: ignoring incompatible override '{}'.", parameter->first)
                continue;
            }
            switch (parameter->second.Type)
            {
                case SMaterialParam::EType::Scalar:
                    instance->SetScalar(parameter->first, parameter->second.Scalar);
                    break;
                case SMaterialParam::EType::Vector:
                    instance->SetVector(parameter->first, parameter->second.Vector);
                    break;
                case SMaterialParam::EType::Texture:
                    instance->SetTexture(parameter->first, parameter->second.TextureRef);
                    break;
            }
        }
        EE_CORE_INFO("Material instance loaded from '{}'.", path.string())
        return instance;
    }

}
