#include "epch.h"
#include "MaterialGraphDocument.h"

#include "Material.h"

#include <simdjson.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <unordered_set>

namespace Elixir
{
    namespace fs = std::filesystem;

    namespace
    {
        // The HLSL accessor for a texture slot (0=base..4=occlusion), matching the
        // bound texture array in the template shader.
        std::string TextureIndexAccessor(const int slot)
        {
            switch (slot)
            {
                case 0:  return "mat.TexIndex0.x";
                case 1:  return "mat.TexIndex0.y";
                case 2:  return "mat.TexIndex0.z";
                case 3:  return "mat.TexIndex0.w";
                default: return "mat.TexIndex1.x";
            }
        }

        // Reads a number tolerant of integer vs floating-point tokens.
        double Number(const simdjson::dom::element element, const double fallback)
        {
            double value;
            if (element.get(value) == simdjson::SUCCESS) return value;
            int64_t integer;
            if (element.get(integer) == simdjson::SUCCESS) return (double)integer;
            return fallback;
        }

        void CopyText(char* destination, const size_t capacity, const std::string_view value)
        {
            const size_t length = std::min(value.size(), capacity - 1);
            std::memcpy(destination, value.data(), length);
            destination[length] = '\0';
        }

        // Where a FunctionCall's sub-graph lives. Material assets are the current
        // format; .matgraph.json is what the editor wrote before graphs became assets.
        std::optional<SMaterialGraphDocument> LoadFunction(const std::string& name)
        {
            const std::string directory = "./Assets/Materials/";
            for (const std::string_view suffix : { ".material.json", ".matgraph.json" })
                if (auto document = MaterialGraphDocument::Load(directory + name + std::string(suffix), false))
                    return document;
            return std::nullopt;
        }
    }

    void SMaterialGraphDocument::Expand(std::vector<SMaterialGraphNode>& outNodes, int outChannels[11]) const
    {
        outNodes = Nodes;
        for (int i = 0; i < 11; ++i) outChannels[i] = Channels[i];

        int remapBase = 1000000;
        constexpr int MAX_EXPANSIONS = 256; // guards against recursive/cyclic functions
        int guard = 0;
        std::unordered_set<int> failedCalls;

        // Expand one FunctionCall per iteration until none remain. Because expanding a
        // function may introduce its own FunctionCalls (nesting), this naturally
        // recurses -- newly added calls are picked up on later iterations.
        while (guard++ < MAX_EXPANSIONS)
        {
            int callIdx = -1;
            for (size_t k = 0; k < outNodes.size(); ++k)
                if (outNodes[k].Type == EMaterialNodeType::FunctionCall
                    && !failedCalls.contains(outNodes[k].Id)) { callIdx = (int)k; break; }
            if (callIdx < 0)
                break; // fully expanded

            const SMaterialGraphNode call = outNodes[callIdx]; // copy: outNodes is mutated below
            if (std::find(call.FunctionStack.begin(), call.FunctionStack.end(), call.Param) != call.FunctionStack.end())
            {
                failedCalls.insert(call.Id);
                continue;
            }

            int callOut = -1; // the id the call resolves to (-1 = missing/empty function)
            bool expanded = false;

            const auto function = LoadFunction(call.Param);
            const bool hasOutput = function && function->Channels[0] >= 0
                && std::any_of(function->Nodes.begin(), function->Nodes.end(),
                    [&](const SMaterialGraphNode& node)
                    { return node.Id == function->Channels[0] && node.Type != EMaterialNodeType::FunctionInput; });
            if (hasOutput)
            {
                const int base = remapBase;
                remapBase += 100000;
                const auto remap = [&](int id) { return id < 0 ? -1 : base + id; };

                std::unordered_map<int, int> inputIdToSlot;
                for (const auto& fn : function->Nodes)
                    if (fn.Type == EMaterialNodeType::FunctionInput)
                        inputIdToSlot[fn.Id] = fn.TexSlot;

                for (const auto& fn : function->Nodes)
                {
                    if (fn.Type == EMaterialNodeType::FunctionInput)
                        continue; // substituted by the call's inputs
                    SMaterialGraphNode c = fn;
                    c.Id = remap(fn.Id);
                    c.DiagnosticId = call.DiagnosticId >= 0 ? call.DiagnosticId : call.Id;
                    c.FunctionStack = call.FunctionStack;
                    c.FunctionStack.push_back(call.Param);
                    for (int i = 0; i < 3; ++i)
                    {
                        const int src = fn.Inputs[i];
                        if (src < 0) { c.Inputs[i] = -1; continue; }
                        if (const auto it = inputIdToSlot.find(src); it != inputIdToSlot.end())
                        {
                            const int slot = it->second;
                            c.Inputs[i] = (slot >= 0 && slot < 3) ? call.Inputs[slot] : -1; // caller's input
                        }
                        else
                        {
                            c.Inputs[i] = remap(src);
                        }
                    }
                    outNodes.push_back(c);
                }
                callOut = remap(function->Channels[0]); // a function returns its Base Color driver
                expanded = true;
            }

            if (!expanded)
            {
                failedCalls.insert(call.Id);
                continue;
            }

            // Redirect references to this call to its output, then remove the call.
            for (auto& n : outNodes)
                for (int i = 0; i < 3; ++i)
                    if (n.Inputs[i] == call.Id) n.Inputs[i] = callOut;
            for (int ch = 0; ch < 11; ++ch)
                if (outChannels[ch] == call.Id) outChannels[ch] = callOut;
            outNodes.erase(std::remove_if(outNodes.begin(), outNodes.end(),
                [&](const SMaterialGraphNode& n)
                { return n.Type == EMaterialNodeType::FunctionCall && n.Id == call.Id; }),
                outNodes.end());
        }

        if (guard >= MAX_EXPANSIONS)
            EE_CORE_ERROR("Material graph: function expansion hit the limit ({}); recursive functions?", MAX_EXPANSIONS)

        // Leftover function placeholders deliberately survive into MaterialGraph so
        // Validate() can report them on the visible node instead of silently replacing
        // the call with zero.
    }

    MaterialGraph SMaterialGraphDocument::Build(
        const std::unordered_map<std::string, bool>& staticOverrides) const
    {
        std::vector<SMaterialGraphNode> nodes;
        int channels[11];
        Expand(nodes, channels);

        MaterialGraph graph;
        std::unordered_map<int, uint32_t> idMap;

        int paramSlot = 0;
        for (const auto& n : nodes)
        {
            SMaterialNode gn;
            gn.Type = n.Type;
            gn.SourceId = (uint32_t)(n.DiagnosticId >= 0 ? n.DiagnosticId : n.Id);
            gn.OutputType = n.OutputType;
            gn.ConstantValue = n.Constant;
            gn.ParameterName = n.Param;
            gn.TextureSampleType = n.TextureSampleType;
            gn.TextureSampleAddress = n.TextureSampleAddress;
            gn.TextureSampleFilter = n.TextureSampleFilter;
            gn.TextureSampleMipMode = n.TextureSampleMipMode;
            gn.TextureSampleOutput = n.TextureSampleOutput;
            if (n.Type == EMaterialNodeType::StaticBoolParameter)
            {
                const auto override = staticOverrides.find(n.Param);
                if (override != staticOverrides.end())
                    gn.ConstantValue.x = override->second ? 1.0f : 0.0f;
            }
            const int inputCount = std::clamp(n.InputCount, 0, 3);
            gn.Inputs.assign(inputCount, -1);
            if (n.Type == EMaterialNodeType::TextureSample)
                gn.TextureExpression = TextureIndexAccessor(n.TexSlot);
            if (n.Type == EMaterialNodeType::Custom)
                gn.CustomCode = n.Code;
            // Exposed parameters get a GraphParams slot in node order. Validate()
            // reports nodes beyond the shader's fixed array instead of aliasing slot 7.
            if (n.Type == EMaterialNodeType::ParamScalar || n.Type == EMaterialNodeType::ParamColor
                || n.Type == EMaterialNodeType::TextureParameter
                || n.Type == EMaterialNodeType::TextureObjectParameter)
                gn.ParamSlot = paramSlot++;
            idMap[n.Id] = graph.AddNode(gn);
        }

        for (const auto& n : nodes)
            for (int i = 0; i < std::clamp(n.InputCount, 0, 3); ++i)
            {
                if (n.Inputs[i] < 0)
                    continue;
                const auto source = idMap.find(n.Inputs[i]);
                const uint32_t sourceId = source != idMap.end()
                    ? source->second : (0x80000000u ^ (uint32_t)n.Inputs[i]);
                graph.Connect(sourceId, idMap[n.Id], (uint32_t)i);
            }

        for (int ch = 0; ch < 11; ++ch)
        {
            if (channels[ch] < 0)
                continue;
            const auto source = idMap.find(channels[ch]);
            const uint32_t sourceId = source != idMap.end()
                ? source->second : (0x80000000u ^ (uint32_t)channels[ch]);
            graph.SetChannel((EMaterialChannel)ch, sourceId);
        }

        graph.SetBlend((EMaterialBlendMode)BlendMode, AlphaCutoff);
        graph.SetShadingModel((EMaterialShadingModel)ShadingModel);
        return graph;
    }

    Ref<Material> SMaterialGraphDocument::BuildMaterial(
        const std::string& name, const Ref<Material>& base) const
    {
        auto material = CreateRef<Material>(name.empty() ? "GraphMaterial" : name);
        if (base)
        {
            std::unordered_set<std::string> oldGraphParameters;
            for (const auto& parameter : base->GetGraphParameters())
                oldGraphParameters.insert(parameter.Name);
            for (const auto& [parameterName, value] : base->GetDefaults())
                if (!oldGraphParameters.contains(parameterName))
                    material->SetDefault(parameterName, value);
        }

        std::vector<SMaterialGraphNode> nodes;
        int channels[11];
        Expand(nodes, channels);

        std::vector<SMaterialGraphParameter> layout;
        uint32_t slot = 0;
        for (const auto& node : nodes)
        {
            if (node.Type == EMaterialNodeType::StaticBoolParameter)
            {
                material->SetStaticDefault(node.Param, node.Constant.x >= 0.5f);
                continue;
            }
            if (node.Type == EMaterialNodeType::TextureParameter
                || node.Type == EMaterialNodeType::TextureObjectParameter)
            {
                const SMaterialParam* inherited = base ? base->GetDefault(node.Param) : nullptr;
                if (inherited && inherited->Type == SMaterialParam::EType::Texture)
                    material->SetDefault(node.Param, *inherited);
                else
                    material->SetDefault(node.Param, SMaterialParam::MakeTexture(nullptr));
                layout.push_back({ node.Param, SMaterialParam::EType::Texture, slot++ });
                continue;
            }
            if (node.Type != EMaterialNodeType::ParamScalar && node.Type != EMaterialNodeType::ParamColor)
                continue;

            const auto type = node.Type == EMaterialNodeType::ParamScalar
                ? SMaterialParam::EType::Scalar : SMaterialParam::EType::Vector;
            if (type == SMaterialParam::EType::Scalar)
                material->SetDefault(node.Param, SMaterialParam::MakeScalar(node.Constant.x));
            else
                material->SetDefault(node.Param, SMaterialParam::MakeVector(node.Constant));
            layout.push_back({ node.Param, type, slot++ });
        }

        material->SetGraph(Build(), std::move(layout));
        // Every graph material carries the document it came from, so saving one never
        // has to go looking for the authoring state that produced it.
        SMaterialGraphDocument authored = *this;
        authored.Overrides.clear();
        authored.StaticValues.clear();
        material->SetDocument(std::move(authored));
        return material;
    }

    void SMaterialGraphDocument::ApplyParametersTo(MaterialInstance& instance) const
    {
        std::vector<SMaterialGraphNode> nodes;
        int channels[11];
        Expand(nodes, channels);
        for (const auto& node : nodes)
        {
            if (node.Type == EMaterialNodeType::StaticBoolParameter)
            {
                const auto value = StaticValues.find(node.Param);
                if (value != StaticValues.end())
                    instance.SetStaticBool(node.Param, value->second);
                continue;
            }
            if (node.Type != EMaterialNodeType::ParamScalar && node.Type != EMaterialNodeType::ParamColor)
                continue;
            const auto value = Overrides.find(node.Param);
            const glm::vec4 resolved = value != Overrides.end() ? value->second : node.Constant;
            if (node.Type == EMaterialNodeType::ParamScalar)
                instance.SetScalar(node.Param, resolved.x);
            else
                instance.SetVector(node.Param, resolved);
        }
    }

    size_t SMaterialGraphDocument::StructHash() const
    {
        size_t h = 1469598103934665603ull;
        const auto mix = [&](size_t v) { h ^= v; h *= 1099511628211ull; };
        const auto mixf = [&](float f) { uint32_t u = 0; std::memcpy(&u, &f, sizeof(u)); mix(u); };
        for (const auto& n : Nodes)
        {
            mix((size_t)n.Type); mix((size_t)n.Id); mix((size_t)n.InputCount); mix((size_t)n.TexSlot);
            mix((size_t)n.TextureSampleType); mix((size_t)n.TextureSampleAddress);
            mix((size_t)n.TextureSampleFilter); mix((size_t)n.TextureSampleMipMode);
            mix((size_t)n.TextureSampleOutput);
            mixf(n.Constant.x); mixf(n.Constant.y); mixf(n.Constant.z); mixf(n.Constant.w);
            for (int i = 0; i < 3; ++i) mix((size_t)(n.Inputs[i] + 2));
            for (const char* p = n.Param; *p; ++p) mix((size_t)*p);
            for (const char* p = n.Code; *p; ++p) mix((size_t)*p);
        }
        for (int i = 0; i < 11; ++i) mix((size_t)(Channels[i] + 2));
        mix((size_t)TargetMaterial); mix((size_t)BlendMode); mix((size_t)ShadingModel);
        mixf(AlphaCutoff);
        std::vector<std::pair<std::string, bool>> staticValues(StaticValues.begin(), StaticValues.end());
        std::sort(staticValues.begin(), staticValues.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [name, value] : staticValues)
        {
            for (const char c : name) mix((size_t)c);
            mix(value ? 1u : 0u);
        }
        return h;
    }

    size_t SMaterialGraphDocument::Hash() const
    {
        // StructHash() plus the state it deliberately ignores: an undo step must also
        // capture moving a node, retitling a comment, or nudging an exposed parameter.
        size_t h = StructHash();
        const auto mix = [&](size_t v) { h ^= v; h *= 1099511628211ull; };
        const auto mixf = [&](float f) { uint32_t u = 0; std::memcpy(&u, &f, sizeof(u)); mix(u); };
        for (const auto& n : Nodes)
        {
            mixf(n.Pos.x); mixf(n.Pos.y);
        }
        for (const auto& [name, v] : Overrides)
        {
            for (char c : name) mix((size_t)c);
            mixf(v.x); mixf(v.y); mixf(v.z); mixf(v.w);
        }
        for (const auto& [name, value] : StaticValues)
        {
            for (const char c : name) mix((size_t)c);
            mix(value ? 1u : 0u);
        }
        for (const auto& c : Comments)
        {
            mixf(c.Pos.x); mixf(c.Pos.y); mixf(c.Size.x); mixf(c.Size.y);
            for (const char* p = c.Text; *p; ++p) mix((size_t)*p);
        }
        return h;
    }

    void MaterialGraphDocument::WriteFields(
        std::ostream& out, const SMaterialGraphDocument& document, const std::string& indent)
    {
        const std::string nested = indent + "  ";
        out << indent << "\"blendMode\": " << document.BlendMode << ",\n";
        out << indent << "\"alphaCutoff\": " << document.AlphaCutoff << ",\n";
        out << indent << "\"shadingModel\": " << document.ShadingModel << ",\n";
        out << indent << "\"nextId\": " << document.NextId << ",\n";
        out << indent << "\"channels\": [";
        for (int i = 0; i < 11; ++i) out << (i ? ", " : "") << document.Channels[i];
        out << "],\n";
        out << indent << "\"nodes\": [\n";
        for (size_t k = 0; k < document.Nodes.size(); ++k)
        {
            const SMaterialGraphNode& n = document.Nodes[k];
            out << nested << "{ "
                << "\"id\": " << n.Id << ", "
                << "\"type\": " << (int)n.Type << ", "
                << "\"outputType\": " << (int)n.OutputType << ", "
                << "\"pos\": [" << n.Pos.x << ", " << n.Pos.y << "], "
                << "\"constant\": [" << n.Constant.x << ", " << n.Constant.y << ", "
                << n.Constant.z << ", " << n.Constant.w << "], "
                << "\"param\": \"" << n.Param << "\", "
                << "\"code\": \"" << n.Code << "\", "
                << "\"texSlot\": " << n.TexSlot << ", "
                << "\"sampleType\": " << (int)n.TextureSampleType << ", "
                << "\"sampleAddress\": " << (int)n.TextureSampleAddress << ", "
                << "\"sampleFilter\": " << (int)n.TextureSampleFilter << ", "
                << "\"sampleMip\": " << (int)n.TextureSampleMipMode << ", "
                << "\"sampleOutput\": " << (int)n.TextureSampleOutput << ", "
                << "\"inputCount\": " << n.InputCount << ", "
                << "\"inputs\": [" << n.Inputs[0] << ", " << n.Inputs[1] << ", " << n.Inputs[2] << "] }"
                << (k + 1 < document.Nodes.size() ? "," : "") << "\n";
        }
        out << indent << "],\n";

        // Comment boxes.
        out << indent << "\"comments\": [";
        for (size_t i = 0; i < document.Comments.size(); ++i)
        {
            const SMaterialGraphComment& c = document.Comments[i];
            out << (i ? ", " : "") << "{ \"pos\": [" << c.Pos.x << ", " << c.Pos.y << "], \"size\": ["
                << c.Size.x << ", " << c.Size.y << "], \"text\": \"" << c.Text << "\" }";
        }
        out << ']';
    }

    bool MaterialGraphDocument::Save(const fs::path& path, const SMaterialGraphDocument& document)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream out(path);
        if (!out)
        {
            EE_CORE_ERROR("Material graph: failed to open '{}' for writing.", path.string())
            return false;
        }

        out << "{\n  \"version\": 1,\n";
        WriteFields(out, document);
        out << "\n}\n";

        EE_CORE_INFO("Material graph saved to '{}'.", path.string())
        return true;
    }

    std::optional<SMaterialGraphDocument> MaterialGraphDocument::Load(
        const std::filesystem::path& path, const bool logErrors)
    {
        auto json = simdjson::padded_string::load(path.string());
        if (json.error())
        {
            if (logErrors)
                EE_CORE_ERROR("Material graph: failed to open '{}': {}", path.string(),
                    simdjson::error_message(json.error()))
            return std::nullopt;
        }

        simdjson::dom::parser parser;
        simdjson::dom::element doc;
        if (const auto error = parser.parse(json.value()).get(doc))
        {
            if (logErrors)
                EE_CORE_ERROR("Material graph: failed to parse '{}': {}", path.string(),
                    simdjson::error_message(error))
            return std::nullopt;
        }

        SMaterialGraphDocument document;
        int64_t integer;
        if (doc["targetMaterial"].get(integer) == simdjson::SUCCESS) document.TargetMaterial = (int)integer;
        if (doc["nextId"].get(integer) == simdjson::SUCCESS) document.NextId = std::max((int)integer, 1);
        if (doc["blendMode"].get(integer) == simdjson::SUCCESS) document.BlendMode = (int)integer;
        if (doc["shadingModel"].get(integer) == simdjson::SUCCESS) document.ShadingModel = (int)integer;
        // Read like every other number here: a whole-numbered cutoff is written as an
        // integer token, so don't depend on it arriving as a floating-point one.
        document.AlphaCutoff = (float)Number(doc["alphaCutoff"], document.AlphaCutoff);

        simdjson::dom::array array;
        if (doc["channels"].get(array) == simdjson::SUCCESS)
        {
            int i = 0;
            for (auto c : array) { if (i < 11) document.Channels[i] = (int)Number(c, -1.0); ++i; }
        }

        if (doc["nodes"].get(array) == simdjson::SUCCESS)
        {
            for (auto e : array)
            {
                SMaterialGraphNode node;
                if (e["id"].get(integer) == simdjson::SUCCESS) node.Id = (int)integer;
                node.DiagnosticId = node.Id;
                if (e["type"].get(integer) == simdjson::SUCCESS) node.Type = (EMaterialNodeType)integer;
                if (e["outputType"].get(integer) == simdjson::SUCCESS) node.OutputType = (EGraphValueType)integer;
                if (e["texSlot"].get(integer) == simdjson::SUCCESS) node.TexSlot = (int)integer;
                if (e["sampleType"].get(integer) == simdjson::SUCCESS)
                    node.TextureSampleType = (ETextureSampleType)integer;
                if (e["sampleAddress"].get(integer) == simdjson::SUCCESS)
                    node.TextureSampleAddress = (ETextureSampleAddress)integer;
                if (e["sampleFilter"].get(integer) == simdjson::SUCCESS)
                    node.TextureSampleFilter = (ETextureSampleFilter)integer;
                if (e["sampleMip"].get(integer) == simdjson::SUCCESS)
                    node.TextureSampleMipMode = (ETextureSampleMipMode)integer;
                if (e["sampleOutput"].get(integer) == simdjson::SUCCESS)
                    node.TextureSampleOutput = (ETextureSampleOutput)integer;
                if (e["inputCount"].get(integer) == simdjson::SUCCESS)
                    node.InputCount = std::clamp((int)integer, 0, 3);
                if (node.Type == EMaterialNodeType::TextureObjectSample)
                    node.InputCount = 3; // migrate the original Texture + UV form

                simdjson::dom::array components;
                if (e["pos"].get(components) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : components) { if (i < 2) (&node.Pos.x)[i] = (float)Number(v, 0.0); ++i; }
                }
                if (e["constant"].get(components) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : components) { if (i < 4) (&node.Constant.x)[i] = (float)Number(v, 0.0); ++i; }
                }
                std::string_view text;
                if (e["param"].get(text) == simdjson::SUCCESS) CopyText(node.Param, sizeof(node.Param), text);
                if (e["code"].get(text) == simdjson::SUCCESS) CopyText(node.Code, sizeof(node.Code), text);
                if (e["inputs"].get(components) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : components) { if (i < 3) node.Inputs[i] = (int)Number(v, -1.0); ++i; }
                }
                document.Nodes.push_back(node);
            }
        }

        // Exposed-parameter values are the instance's to carry, so they are never
        // written here; graphs saved before that split still have them.
        if (doc["paramOverrides"].get(array) == simdjson::SUCCESS)
        {
            for (auto o : array)
            {
                std::string_view name;
                simdjson::dom::array value;
                if (o["name"].get(name) != simdjson::SUCCESS || o["value"].get(value) != simdjson::SUCCESS)
                    continue;
                glm::vec4 v(0.0f);
                int i = 0;
                for (auto c : value) { if (i < 4) (&v.x)[i] = (float)Number(c, 0.0); ++i; }
                document.Overrides[std::string(name)] = v;
            }
        }

        if (doc["comments"].get(array) == simdjson::SUCCESS)
        {
            for (auto e : array)
            {
                SMaterialGraphComment comment;
                simdjson::dom::array components;
                if (e["pos"].get(components) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : components) { if (i < 2) (&comment.Pos.x)[i] = (float)Number(v, 0.0); ++i; }
                }
                if (e["size"].get(components) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : components) { if (i < 2) (&comment.Size.x)[i] = (float)Number(v, 0.0); ++i; }
                }
                std::string_view text;
                if (e["text"].get(text) == simdjson::SUCCESS)
                    CopyText(comment.Text, sizeof(comment.Text), text);
                document.Comments.push_back(comment);
            }
        }

        return document;
    }
}
