#include "VFXDocument.h"

#include <simdjson.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <fstream>
#include <iterator>
#include <utility>

namespace EditorVFX
{
    namespace
    {
        std::string Quote(const std::string& text)
        {
            const char* hex = "0123456789abcdef";
            std::string result = "\"";
            for (const unsigned char c : text)
            {
                if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
                else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
                else result += static_cast<char>(c);
            }
            return result + '"';
        }

        Value Copy(simdjson::dom::element element)
        {
            Value result;
            switch (element.type())
            {
                case simdjson::dom::element_type::OBJECT:
                    result.Type = EValueType::Object;
                    for (const auto field : element.get_object().value())
                        result.Object.emplace(std::string(field.key), Copy(field.value));
                    break;
                case simdjson::dom::element_type::ARRAY:
                    result.Type = EValueType::Array;
                    for (const auto item : element.get_array().value()) result.Array.push_back(Copy(item));
                    break;
                case simdjson::dom::element_type::STRING:
                    result.Type = EValueType::String;
                    result.Text = element.get_string().value();
                    break;
                case simdjson::dom::element_type::BOOL:
                    result.Type = EValueType::Boolean;
                    result.Text = element.get_bool().value() ? "true" : "false";
                    break;
                case simdjson::dom::element_type::NULL_VALUE: break;
                default:
                    result.Type = EValueType::Number;
                    result.Text = simdjson::to_string(element);
                    break;
            }
            return result;
        }

        Value* At(Value& root, const Path& path)
        {
            auto value = &root;
            for (const auto& segment : path)
            {
                if (value->Type == EValueType::Object)
                {
                    const auto it = value->Object.find(segment);
                    if (it == value->Object.end()) return nullptr;
                    value = &it->second;
                }
                else if (value->Type == EValueType::Array)
                {
                    size_t index = 0;
                    const auto [end, error] = std::from_chars(segment.data(), segment.data() + segment.size(), index);
                    if (error != std::errc{} || end != segment.data() + segment.size() || index >= value->Array.size()) return nullptr;
                    value = &value->Array[index];
                }
                else return nullptr;
            }
            return value;
        }

        bool Read(const std::filesystem::path& path, std::string& text)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) return false;
            text.assign(std::istreambuf_iterator<char>(stream), {});
            return !stream.bad();
        }

        std::string Pretty(const Value& value, const unsigned depth = 0)
        {
            if (value.Type != EValueType::Object && value.Type != EValueType::Array) return value.JSON();
            if (value.Type == EValueType::Array && std::all_of(value.Array.begin(), value.Array.end(), [](const Value& item)
                { return item.Type != EValueType::Object && item.Type != EValueType::Array; })) return value.JSON();
            const std::string indentation((depth + 1) * 2, ' ');
            std::vector<std::string> fields;
            if (value.Type == EValueType::Object)
                for (const auto& [key, item] : value.Object) fields.push_back(indentation + Quote(key) + ": " + Pretty(item, depth + 1));
            else
                for (const auto& item : value.Array) fields.push_back(indentation + Pretty(item, depth + 1));
            if (fields.empty()) return value.JSON();
            std::string result = value.Type == EValueType::Object ? "{\n" : "[\n";
            for (size_t index = 0; index < fields.size(); ++index)
                result += fields[index] + (index + 1 < fields.size() ? ",\n" : "\n");
            return result + std::string(depth * 2, ' ') + (value.Type == EValueType::Object ? '}' : ']');
        }

        bool ValidateRoot(const Value& root, std::string& error)
        {
            const auto name = root.Find("name");
            const auto emitters = root.Find("emitters");
            if (root.Type != EValueType::Object || !name || name->Type != EValueType::String || name->Text.empty() ||
                !emitters || emitters->Type != EValueType::Array)
            {
                error = "An effect needs a name and an emitters array.";
                return false;
            }
            std::vector<std::string> names;
            for (const auto& emitter : emitters->Array)
            {
                const auto emitterName = emitter.String("name");
                if (emitter.Type != EValueType::Object || emitterName.empty() ||
                    std::find(names.begin(), names.end(), emitterName) != names.end())
                {
                    error = "Emitter names must be nonempty and unique.";
                    return false;
                }
                names.push_back(emitterName);
            }
            return true;
        }
    }

    const Value* Value::Find(const std::string& key) const
    {
        const auto it = Object.find(key);
        return it == Object.end() ? nullptr : &it->second;
    }

    std::string Value::String(const std::string& key, const std::string& fallback) const
    {
        const auto value = Find(key);
        return value && value->Type == EValueType::String ? value->Text : fallback;
    }

    bool Value::Enabled() const
    {
        const auto value = Find("enabled");
        return !value || value->Type != EValueType::Boolean || value->Text != "false";
    }

    Value Value::FromString(std::string text)
    {
        Value result;
        result.Type = EValueType::String;
        result.Text = std::move(text);
        return result;
    }

    std::string Value::JSON() const
    {
        if (Type == EValueType::String) return Quote(Text);
        if (Type == EValueType::Null) return "null";
        if (Type == EValueType::Boolean || Type == EValueType::Number) return Text;
        std::string result = Type == EValueType::Array ? "[" : "{";
        bool first = true;
        if (Type == EValueType::Array)
            for (const auto& value : Array) { if (!first) result += ','; first = false; result += value.JSON(); }
        else
            for (const auto& [key, value] : Object) { if (!first) result += ','; first = false; result += Quote(key) + ':' + value.JSON(); }
        return result + (Type == EValueType::Array ? ']' : '}');
    }

    bool Value::Parse(const std::string& text, Value& result, std::string& error)
    {
        simdjson::dom::parser parser;
        simdjson::dom::element element;
        const auto status = parser.parse(text).get(element);
        if (status) { error = simdjson::error_message(status); return false; }
        result = Copy(element);
        error.clear();
        return true;
    }

    bool Document::Load(const std::filesystem::path& path, std::string& error)
    {
        std::string text;
        Value root;
        if (!Read(path, text)) { error = "Could not read " + path.string(); return false; }
        if (!Value::Parse(text, root, error) || !ValidateRoot(root, error)) return false;
        m_Root = std::move(root);
        m_Path = path;
        m_Saved = m_Root.JSON();
        m_DiskText = std::move(text);
        m_ExistsOnDisk = true;
        m_Undo.clear(); m_Redo.clear();
        return true;
    }

    bool Document::Create(const std::filesystem::path& path, const std::string& json, std::string& error)
    {
        Value root;
        if (!Value::Parse(json, root, error) || !ValidateRoot(root, error)) return false;
        m_Root = std::move(root); m_Path = path;
        m_Saved.clear(); m_DiskText.clear(); m_ExistsOnDisk = false;
        m_Undo.clear(); m_Redo.clear();
        return true;
    }

    const Value* Document::Get(const Path& path) const
    {
        return At(const_cast<Value&>(m_Root), path);
    }

    void Document::Commit(Value root)
    {
        if (root.JSON() == m_Root.JSON()) return;
        if (m_Undo.size() == 100) m_Undo.erase(m_Undo.begin());
        m_Undo.push_back(m_Root); m_Redo.clear(); m_Root = std::move(root);
    }

    bool Document::Apply(const std::vector<Edit>& edits, std::string& error)
    {
        auto root = m_Root;
        for (const auto& edit : edits)
        {
            const auto value = At(root, edit.Location);
            if (!value) { error = "The selected property no longer exists."; return false; }
            Value replacement;
            if (value->Type == EValueType::String && (value->Text.empty() || value->Text.front() != '$') &&
                (edit.Text.empty() || edit.Text.front() != '"'))
                replacement = Value::FromString(edit.Text);
            else if (!edit.Text.empty() && edit.Text.front() == '$')
                replacement = Value::FromString(edit.Text);
            else if (!Value::Parse(edit.Text, replacement, error)) return false;
            *value = std::move(replacement);
        }
        if (!ValidateRoot(root, error)) return false;
        Commit(std::move(root));
        return true;
    }

    bool Document::Set(const Path& path, Value value)
    {
        auto root = m_Root;
        if (auto target = At(root, path)) *target = std::move(value);
        else if (!path.empty())
        {
            auto parentPath = path; const auto key = parentPath.back(); parentPath.pop_back();
            auto parent = At(root, parentPath);
            if (!parent || parent->Type != EValueType::Object) return false;
            parent->Object[key] = std::move(value);
        }
        else return false;
        Commit(std::move(root)); return true;
    }

    bool Document::Append(const Path& path, Value value)
    {
        auto root = m_Root; const auto array = At(root, path);
        if (!array || array->Type != EValueType::Array) return false;
        array->Array.push_back(std::move(value)); Commit(std::move(root)); return true;
    }

    bool Document::Remove(const Path& path, const size_t index)
    {
        auto root = m_Root; const auto array = At(root, path);
        if (!array || index >= array->Array.size()) return false;
        array->Array.erase(array->Array.begin() + static_cast<std::ptrdiff_t>(index)); Commit(std::move(root)); return true;
    }

    bool Document::Move(const Path& path, const size_t index, const int direction)
    {
        auto root = m_Root; const auto array = At(root, path);
        if (!array || index >= array->Array.size() || (direction != -1 && direction != 1)) return false;
        if ((direction < 0 && index == 0) || (direction > 0 && index + 1 >= array->Array.size())) return false;
        std::swap(array->Array[index], array->Array[direction < 0 ? index - 1 : index + 1]);
        Commit(std::move(root)); return true;
    }

    bool Document::Undo()
    {
        if (m_Undo.empty()) return false;
        m_Redo.push_back(m_Root); m_Root = std::move(m_Undo.back()); m_Undo.pop_back(); return true;
    }

    bool Document::Redo()
    {
        if (m_Redo.empty()) return false;
        m_Undo.push_back(m_Root); m_Root = std::move(m_Redo.back()); m_Redo.pop_back(); return true;
    }

    bool Document::Save(std::string& error)
    {
        std::string disk;
        std::error_code status;
        if ((m_ExistsOnDisk && (!Read(m_Path, disk) || disk != m_DiskText)) ||
            (!m_ExistsOnDisk && std::filesystem::exists(m_Path, status)))
        {
            error = "The asset changed on disk. Reload it before saving."; return false;
        }
        if (status) { error = status.message(); return false; }
        const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto temporary = std::filesystem::path(m_Path.string() + "." + suffix + ".tmp");
        const auto backup = std::filesystem::path(m_Path.string() + "." + suffix + ".backup");
        const auto json = Pretty(m_Root) + '\n';
        {
            std::ofstream stream(temporary, std::ios::binary);
            stream << json; stream.close();
            if (!stream) { std::filesystem::remove(temporary, status); error = "Could not write the temporary asset."; return false; }
        }
        if (m_ExistsOnDisk) std::filesystem::rename(m_Path, backup, status);
        if (!status) std::filesystem::rename(temporary, m_Path, status);
        if (status)
        {
            error = "Could not save asset: " + status.message();
            std::error_code cleanup;
            if (std::filesystem::exists(backup, cleanup))
            {
                std::filesystem::rename(backup, m_Path, cleanup);
                if (cleanup) error += ". Original asset preserved at " + backup.string();
            }
            std::filesystem::remove(temporary, cleanup); return false;
        }
        if (m_ExistsOnDisk) std::filesystem::remove(backup, status);
        m_DiskText = json; m_Saved = m_Root.JSON(); m_ExistsOnDisk = true;
        error.clear(); return true;
    }

    Value MakeEmitter(const std::string& name)
    {
        Value emitter; std::string error;
        Value::Parse(R"({"name":"Emitter","enabled":true,"renderMode":"Sprite","maxParticles":1000,"spawnRate":100,"material":{"color":[1,0.65,0.2,1]},"parameters":{},"spawnModules":[{"type":"SetLifetime","min":1,"max":2},{"type":"SetPositionBox","min":[-0.1,0,-0.1],"max":[0.1,0.1,0.1]},{"type":"SetVelocityCone","direction":[0,1,0],"angle":0.35,"minSpeed":1,"maxSpeed":2},{"type":"SetSize","min":0.04,"max":0.1},{"type":"SetColor","color":[1,0.65,0.2,1]}],"updateModules":[{"type":"ApplyGravity","gravity":[0,-0.5,0,0]}]})", emitter, error);
        emitter.Object["name"] = Value::FromString(name); return emitter;
    }

    std::vector<Value> ModuleTemplates(const bool spawn)
    {
        const std::vector<std::string> definitions = spawn ? std::vector<std::string>{
            R"({"type":"SetLifetime","min":1,"max":2})", R"({"type":"SetSize","min":0.05,"max":0.1})",
            R"({"type":"SetColor","color":[1,1,1,1]})", R"({"type":"SetPositionBox","min":[-0.5,0,-0.5],"max":[0.5,0.5,0.5]})",
            R"({"type":"SetPositionDisk","center":[0,0,0],"radius":0.5})",
            R"({"type":"SetVelocityCone","direction":[0,1,0],"angle":0.35,"minSpeed":1,"maxSpeed":2})",
            R"({"type":"SetRotation","min":0,"max":6.2831853})", R"({"type":"SetScale","min":0.8,"max":1.2})",
            R"({"type":"SetRibbonId","ribbonId":0})"
        } : std::vector<std::string>{
            R"({"type":"ApplyGravity","target":"Particle","gravity":[0,-9.8,0,0]})", R"({"type":"ApplyLinearDrag","drag":0.1})",
            R"({"type":"ApplyCurlNoise","target":"Particle","strength":0.65,"frequency":2.2,"scrollVelocity":[0.12,0.09,0.07],"seed":0})",
            R"({"type":"SizeOverLife","start":0.1,"end":0.01})", R"({"type":"ScaleOverLife","start":1,"end":0})",
            R"({"type":"ApplyAngularVelocity","value":1})", R"({"type":"ColorOverLife","start":[1,1,1,1],"end":[1,1,1,0]})",
            R"({"type":"KillOutsideBounds","min":[-10,-10,-10],"max":[10,10,10]})"
        };
        std::vector<Value> result;
        for (const auto& definition : definitions) { Value value; std::string error; if (Value::Parse(definition, value, error)) result.push_back(std::move(value)); }
        return result;
    }
}
