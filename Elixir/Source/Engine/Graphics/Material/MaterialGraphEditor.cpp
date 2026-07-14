#include "epch.h"
#include "MaterialGraphEditor.h"

#include <imgui.h>
#include <simdjson.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace Elixir
{
    namespace
    {
        const char* NodeName(EMaterialNodeType t)
        {
            switch (t)
            {
                case EMaterialNodeType::Constant:      return "Constant";
                case EMaterialNodeType::Scalar:        return "Scalar";
                case EMaterialNodeType::ParamScalar:   return "Param (f)";
                case EMaterialNodeType::ParamColor:    return "Param (rgba)";
                case EMaterialNodeType::Parameter:     return "Parameter";
                case EMaterialNodeType::TextureSample: return "Texture";
                case EMaterialNodeType::TexCoord:      return "TexCoord";
                case EMaterialNodeType::Position:      return "Position";
                case EMaterialNodeType::Time:          return "Time";
                case EMaterialNodeType::Sine:          return "Sine";
                case EMaterialNodeType::Panner:        return "Panner";
                case EMaterialNodeType::Checker:       return "Checker";
                case EMaterialNodeType::Noise:         return "Noise";
                case EMaterialNodeType::Multiply:      return "Multiply";
                case EMaterialNodeType::Add:           return "Add";
                case EMaterialNodeType::Subtract:      return "Subtract";
                case EMaterialNodeType::Divide:        return "Divide";
                case EMaterialNodeType::Power:         return "Power";
                case EMaterialNodeType::Dot:           return "Dot";
                case EMaterialNodeType::Lerp:          return "Lerp";
                case EMaterialNodeType::OneMinus:      return "OneMinus";
                case EMaterialNodeType::Saturate:      return "Saturate";
                case EMaterialNodeType::Fresnel:       return "Fresnel";
            }
            return "Node";
        }

        int InputCountFor(EMaterialNodeType t)
        {
            switch (t)
            {
                case EMaterialNodeType::Multiply:
                case EMaterialNodeType::Add:
                case EMaterialNodeType::Subtract:
                case EMaterialNodeType::Divide:
                case EMaterialNodeType::Power:
                case EMaterialNodeType::Dot:      return 2;
                case EMaterialNodeType::Lerp:     return 3;
                case EMaterialNodeType::OneMinus:
                case EMaterialNodeType::Saturate:
                case EMaterialNodeType::Sine:
                case EMaterialNodeType::Panner:        // input 0 = UV (optional)
                case EMaterialNodeType::Checker:       // input 0 = UV (optional)
                case EMaterialNodeType::Noise:         // input 0 = UV (optional)
                case EMaterialNodeType::TextureSample: return 1; // input 0 = UV (optional)
                default: return 0;
            }
        }

        EGraphValueType OutputTypeFor(EMaterialNodeType t)
        {
            switch (t)
            {
                case EMaterialNodeType::Fresnel:
                case EMaterialNodeType::Dot:
                case EMaterialNodeType::Scalar:
                case EMaterialNodeType::ParamScalar:
                case EMaterialNodeType::Checker:
                case EMaterialNodeType::Noise:
                case EMaterialNodeType::Time:          return EGraphValueType::Float;
                case EMaterialNodeType::TexCoord:
                case EMaterialNodeType::Panner:        return EGraphValueType::Float2;
                case EMaterialNodeType::TextureSample:
                case EMaterialNodeType::Position:      return EGraphValueType::Float3;
                default:                               return EGraphValueType::Float4;
            }
        }

        // The HLSL accessor for a texture slot index (0..4). The codegen wraps this in
        // the NO_TEXTURE guard and pairs it with a UV.
        std::string TextureIndexAccessor(int slot)
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

        const char* const kTextureSlots[] = { "Base Color", "Metal/Rough", "Normal", "Emissive", "Occlusion" };

        const char* ChannelName(int ch)
        {
            switch (ch)
            {
                case 0: return "Base Color";
                case 1: return "Metallic";
                case 2: return "Roughness";
                case 3: return "Emissive";
                case 4: return "Normal";
            }
            return "?";
        }
    }

    MaterialGraphEditor::MaterialGraphEditor()
    {
        // Seed with a simple default graph: Constant * Parameter -> Base Color.
        const int c = AddNode(EMaterialNodeType::Constant, { 40.0f, 40.0f });
        const int p = AddNode(EMaterialNodeType::Parameter, { 40.0f, 150.0f });
        const int m = AddNode(EMaterialNodeType::Multiply, { 240.0f, 90.0f });
        m_Nodes[0].Constant = { 1.0f, 0.5f, 0.1f, 1.0f };
        m_Nodes[2].Inputs[0] = c;
        m_Nodes[2].Inputs[1] = p;
        m_Channels[0] = m;
    }

    int MaterialGraphEditor::AddNode(EMaterialNodeType type, const glm::vec2& pos)
    {
        SNode node;
        node.Id = m_NextId++;
        node.Type = type;
        node.OutputType = OutputTypeFor(type);
        node.Pos = pos;
        node.InputCount = InputCountFor(type);
        // A visible default scale for procedural nodes (Constant.x doubles as scale).
        if (type == EMaterialNodeType::Checker || type == EMaterialNodeType::Noise)
            node.Constant = { 8.0f, 8.0f, 8.0f, 8.0f };
        // Exposed parameters start with a neutral name (edited in the Parameters panel).
        if (type == EMaterialNodeType::ParamScalar || type == EMaterialNodeType::ParamColor)
            std::strncpy(node.Param, type == EMaterialNodeType::ParamScalar ? "Scalar" : "Color", sizeof(node.Param) - 1);
        m_Nodes.push_back(node);
        return node.Id;
    }

    void MaterialGraphEditor::DeleteNode(int id)
    {
        m_Nodes.erase(
            std::remove_if(m_Nodes.begin(), m_Nodes.end(), [id](const SNode& n) { return n.Id == id; }),
            m_Nodes.end());

        // Drop any wires that fed from the deleted node.
        for (auto& n : m_Nodes)
            for (int& src : n.Inputs)
                if (src == id) src = -1;
        for (int& ch : m_Channels)
            if (ch == id) ch = -1;
        if (m_LinkFrom == id)
            m_LinkFrom = -1;
    }

    const MaterialGraphEditor::SNode* MaterialGraphEditor::Find(int id) const
    {
        for (const auto& n : m_Nodes)
            if (n.Id == id) return &n;
        return nullptr;
    }

    bool MaterialGraphEditor::Draw(int materialCount)
    {
        bool apply = false;
        ImGui::Begin("Node Graph Editor");

        const int maxMaterial = materialCount > 0 ? materialCount - 1 : 0;
        if (m_TargetMaterial > maxMaterial) m_TargetMaterial = maxMaterial;
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderInt("Target material", &m_TargetMaterial, 0, maxMaterial);

        // Save / load the graph to ./Assets/Materials/<name>.matgraph.json.
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##file", m_FileName, sizeof(m_FileName));
        ImGui::SameLine();
        const std::string graphPath = std::string("./Assets/Materials/") + m_FileName + ".matgraph.json";
        if (ImGui::Button("Save")) Save(graphPath);
        ImGui::SameLine();
        if (ImGui::Button("Load")) Load(graphPath);
        ImGui::SameLine();
        ImGui::TextDisabled(".matgraph.json");
        ImGui::Separator();

        auto addButton = [&](const char* label, EMaterialNodeType type)
        {
            if (ImGui::Button(label)) AddNode(type, { 40.0f, 40.0f });
            ImGui::SameLine();
        };

        ImGui::TextUnformatted("Inputs:"); ImGui::SameLine();
        addButton("Constant", EMaterialNodeType::Constant);
        addButton("Scalar", EMaterialNodeType::Scalar);
        addButton("Param.f", EMaterialNodeType::ParamScalar);
        addButton("Param.rgba", EMaterialNodeType::ParamColor);
        addButton("Parameter", EMaterialNodeType::Parameter);
        addButton("Texture", EMaterialNodeType::TextureSample);
        addButton("TexCoord", EMaterialNodeType::TexCoord);
        addButton("Position", EMaterialNodeType::Position);
        ImGui::NewLine();

        ImGui::TextUnformatted("Math:  "); ImGui::SameLine();
        addButton("Multiply", EMaterialNodeType::Multiply);
        addButton("Add", EMaterialNodeType::Add);
        addButton("Subtract", EMaterialNodeType::Subtract);
        addButton("Divide", EMaterialNodeType::Divide);
        addButton("Power", EMaterialNodeType::Power);
        addButton("Dot", EMaterialNodeType::Dot);
        addButton("Lerp", EMaterialNodeType::Lerp);
        addButton("OneMinus", EMaterialNodeType::OneMinus);
        addButton("Saturate", EMaterialNodeType::Saturate);
        addButton("Fresnel", EMaterialNodeType::Fresnel);
        ImGui::NewLine();

        ImGui::TextUnformatted("Anim:  "); ImGui::SameLine();
        addButton("Time", EMaterialNodeType::Time);
        addButton("Sine", EMaterialNodeType::Sine);
        addButton("Panner", EMaterialNodeType::Panner);
        ImGui::NewLine();

        ImGui::TextUnformatted("Proc:  "); ImGui::SameLine();
        addButton("Checker", EMaterialNodeType::Checker);
        addButton("Noise", EMaterialNodeType::Noise);
        ImGui::NewLine();

        if (ImGui::Button("Apply")) apply = true;
        ImGui::SameLine();
        ImGui::TextDisabled("(right-click: node = delete, pin = disconnect)");
        ImGui::Separator();

        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float NODE_W = 150.0f;
        const float PIN_R = 6.0f;

        std::unordered_map<int, ImVec2> outPins;   // node id -> output pin pos
        std::unordered_map<long long, ImVec2> inPinPos; // encoded target -> input pin pos

        // Hovered input pin this frame (for connect-on-release). Allowed to register
        // even while the output pin is the active (dragged) item.
        int hovKind = -1, hovId = -1, hovSlot = -1;

        // Node scheduled for deletion this frame (processed after drawing so we don't
        // mutate m_Nodes mid-iteration).
        int deleteId = -1;

        // A pin as an invisible button (robust hit-testing). Returns {active, hovered}.
        auto pinButton = [&](const char* label, const ImVec2& pos)
        {
            ImGui::SetCursorScreenPos(ImVec2(pos.x - PIN_R - 3.0f, pos.y - PIN_R - 3.0f));
            ImGui::InvisibleButton(label, ImVec2((PIN_R + 3.0f) * 2.0f, (PIN_R + 3.0f) * 2.0f));
            const bool active = ImGui::IsItemActive();
            const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            return std::pair<bool, bool>{ active, hovered };
        };

        // --- Nodes ---
        for (auto& node : m_Nodes)
        {
            ImVec2 p = ImVec2(origin.x + node.Pos.x, origin.y + node.Pos.y);
            ImGui::PushID(node.Id);

            // Header acts as the drag handle.
            ImGui::SetCursorScreenPos(p);
            ImGui::InvisibleButton("hdr", ImVec2(NODE_W, 20.0f));
            if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                node.Pos.x += io.MouseDelta.x;
                node.Pos.y += io.MouseDelta.y;
                p = ImVec2(origin.x + node.Pos.x, origin.y + node.Pos.y);
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                deleteId = node.Id;

            const int rows = node.InputCount > 1 ? node.InputCount : 1;
            const float bodyH = 24.0f + rows * 16.0f;
            const ImVec2 rmin = p;
            const ImVec2 rmax = ImVec2(p.x + NODE_W, p.y + 20.0f + bodyH);

            dl->AddRectFilled(rmin, rmax, IM_COL32(42, 42, 52, 235), 4.0f);
            dl->AddRectFilled(rmin, ImVec2(rmax.x, rmin.y + 20.0f), IM_COL32(70, 70, 95, 255), 4.0f);
            dl->AddRect(rmin, rmax, IM_COL32(100, 100, 125, 255), 4.0f);
            dl->AddText(ImVec2(rmin.x + 8.0f, rmin.y + 3.0f), IM_COL32_WHITE, NodeName(node.Type));

            // Value editors.
            ImGui::SetCursorScreenPos(ImVec2(rmin.x + 8.0f, rmin.y + 24.0f));
            ImGui::PushItemWidth(NODE_W - 16.0f);
            if (node.Type == EMaterialNodeType::Constant)
                ImGui::ColorEdit4("##v", &node.Constant.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            else if (node.Type == EMaterialNodeType::Scalar)
                ImGui::DragFloat("##f", &node.Constant.x, 0.05f, -100.0f, 100.0f, "%.3f");
            else if (node.Type == EMaterialNodeType::Parameter
                  || node.Type == EMaterialNodeType::ParamScalar
                  || node.Type == EMaterialNodeType::ParamColor)
                ImGui::InputText("##p", node.Param, sizeof(node.Param));
            else if (node.Type == EMaterialNodeType::TextureSample)
                ImGui::Combo("##t", &node.TexSlot, kTextureSlots, IM_ARRAYSIZE(kTextureSlots));
            else if (node.Type == EMaterialNodeType::Panner)
                ImGui::DragFloat2("##s", &node.Constant.x, 0.01f, -5.0f, 5.0f, "%.2f");
            else if (node.Type == EMaterialNodeType::Checker || node.Type == EMaterialNodeType::Noise)
                ImGui::DragFloat("##sc", &node.Constant.x, 0.1f, 0.1f, 128.0f, "scale %.1f");
            ImGui::PopItemWidth();

            // Output pin (right, centered).
            const ImVec2 outPos = ImVec2(rmax.x, (rmin.y + rmax.y) * 0.5f);
            {
                const auto [active, hovered] = pinButton("out", outPos);
                if (active && m_LinkFrom < 0)
                    m_LinkFrom = node.Id;
                dl->AddCircleFilled(outPos, PIN_R, hovered ? IM_COL32(255, 220, 120, 255) : IM_COL32(220, 190, 90, 255));
            }
            outPins[node.Id] = outPos;

            // Input pins (left edge).
            for (int i = 0; i < node.InputCount; ++i)
            {
                const ImVec2 inPos = ImVec2(rmin.x, rmin.y + 30.0f + i * 16.0f);
                ImGui::PushID(i);
                const auto [active, hovered] = pinButton("in", inPos);
                ImGui::PopID();
                if (hovered)
                {
                    hovKind = 0; hovId = node.Id; hovSlot = i;
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                        node.Inputs[i] = -1;
                }
                const bool connected = node.Inputs[i] >= 0;
                dl->AddCircleFilled(inPos, PIN_R, connected ? IM_COL32(140, 180, 240, 255) : IM_COL32(90, 110, 150, 255));
                inPinPos[((long long)node.Id << 8) | i] = inPos;
            }

            ImGui::PopID();
        }

        // --- Output (master) node ---
        const ImVec2 op = ImVec2(origin.x + 520.0f, origin.y + 40.0f);
        const ImVec2 omin = op;
        const ImVec2 omax = ImVec2(op.x + 140.0f, op.y + 20.0f + 5 * 20.0f + 6.0f);
        dl->AddRectFilled(omin, omax, IM_COL32(52, 42, 42, 235), 4.0f);
        dl->AddRectFilled(omin, ImVec2(omax.x, omin.y + 20.0f), IM_COL32(120, 80, 80, 255), 4.0f);
        dl->AddRect(omin, omax, IM_COL32(140, 100, 100, 255), 4.0f);
        dl->AddText(ImVec2(omin.x + 8.0f, omin.y + 3.0f), IM_COL32_WHITE, "Output");
        ImGui::PushID(-99);
        std::unordered_map<int, ImVec2> chPins;
        for (int ch = 0; ch < 5; ++ch)
        {
            const ImVec2 pinPos = ImVec2(omin.x, omin.y + 30.0f + ch * 20.0f);
            ImGui::PushID(ch);
            const auto [active, hovered] = pinButton("ch", pinPos);
            ImGui::PopID();
            if (hovered)
            {
                hovKind = 1; hovId = ch; hovSlot = 0;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    m_Channels[ch] = -1;
            }
            const bool connected = m_Channels[ch] >= 0;
            dl->AddCircleFilled(pinPos, PIN_R, connected ? IM_COL32(140, 180, 240, 255) : IM_COL32(90, 110, 150, 255));
            dl->AddText(ImVec2(omin.x + 12.0f, pinPos.y - 7.0f), IM_COL32_WHITE, ChannelName(ch));
            chPins[ch] = pinPos;
        }
        ImGui::PopID();

        // Apply a queued deletion now so stale links aren't drawn this frame.
        if (deleteId >= 0)
        {
            DeleteNode(deleteId);
            outPins.erase(deleteId);
        }

        // --- Links ---
        auto bezier = [&](const ImVec2& a, const ImVec2& b, ImU32 col)
        {
            dl->AddBezierCubic(a, ImVec2(a.x + 50.0f, a.y), ImVec2(b.x - 50.0f, b.y), b, col, 2.5f);
        };
        for (const auto& node : m_Nodes)
            for (int i = 0; i < node.InputCount; ++i)
                if (node.Inputs[i] >= 0 && outPins.count(node.Inputs[i]))
                    bezier(outPins[node.Inputs[i]], inPinPos[((long long)node.Id << 8) | i], IM_COL32(200, 200, 210, 200));
        for (int ch = 0; ch < 5; ++ch)
            if (m_Channels[ch] >= 0 && outPins.count(m_Channels[ch]))
                bezier(outPins[m_Channels[ch]], chPins[ch], IM_COL32(220, 190, 90, 220));

        // --- Drag-to-connect ---
        if (m_LinkFrom >= 0 && outPins.count(m_LinkFrom))
            bezier(outPins[m_LinkFrom], io.MousePos, IM_COL32(255, 230, 120, 255));

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_LinkFrom >= 0)
        {
            if (hovKind == 0)
            {
                for (auto& n : m_Nodes)
                    if (n.Id == hovId && hovSlot < n.InputCount)
                        n.Inputs[hovSlot] = m_LinkFrom;
            }
            else if (hovKind == 1)
            {
                m_Channels[hovId] = m_LinkFrom;
            }
            m_LinkFrom = -1;
        }

        ImGui::End();

        // Exposed-parameter panel (own window): editing these pushes live values into
        // the shader (no recompile) via CollectParams() -> cbGraphParams.
        ImGui::Begin("Material Parameters");
        ImGui::TextDisabled("Live -- no recompile. Add Param.f / Param.rgba nodes.");
        ImGui::Separator();
        int slot = 0;
        for (auto& node : m_Nodes)
        {
            if (node.Type != EMaterialNodeType::ParamScalar && node.Type != EMaterialNodeType::ParamColor)
                continue;
            if (slot >= MAX_PARAMS) break;
            ImGui::PushID(1000 + node.Id);
            ImGui::SetNextItemWidth(200.0f);
            if (node.Type == EMaterialNodeType::ParamScalar)
                ImGui::DragFloat(node.Param, &node.Constant.x, 0.01f, -10.0f, 10.0f, "%.3f");
            else
                ImGui::ColorEdit4(node.Param, &node.Constant.x, ImGuiColorEditFlags_AlphaBar);
            ImGui::PopID();
            ++slot;
        }
        if (slot == 0)
            ImGui::TextDisabled("(no exposed parameters)");
        ImGui::End();

        return apply;
    }

    MaterialGraph MaterialGraphEditor::Build() const
    {
        MaterialGraph graph;
        std::unordered_map<int, uint32_t> idMap;

        int paramSlot = 0;
        for (const auto& n : m_Nodes)
        {
            SMaterialNode gn;
            gn.Type = n.Type;
            gn.OutputType = n.OutputType;
            gn.ConstantValue = n.Constant;
            gn.ParameterName = n.Param;
            gn.Inputs.assign(n.InputCount, -1);
            if (n.Type == EMaterialNodeType::TextureSample)
                gn.TextureExpression = TextureIndexAccessor(n.TexSlot);
            // Exposed parameters get a GraphParams slot in node order (capped to the
            // shader's array size); CollectParams() mirrors this ordering.
            if (n.Type == EMaterialNodeType::ParamScalar || n.Type == EMaterialNodeType::ParamColor)
                gn.ParamSlot = paramSlot < MAX_PARAMS ? paramSlot++ : MAX_PARAMS - 1;
            idMap[n.Id] = graph.AddNode(gn);
        }

        for (const auto& n : m_Nodes)
            for (int i = 0; i < n.InputCount; ++i)
                if (n.Inputs[i] >= 0 && idMap.count(n.Inputs[i]))
                    graph.Connect(idMap[n.Inputs[i]], idMap[n.Id], (uint32_t)i);

        for (int ch = 0; ch < 5; ++ch)
            if (m_Channels[ch] >= 0 && idMap.count(m_Channels[ch]))
                graph.SetChannel((EMaterialChannel)ch, idMap[m_Channels[ch]]);

        return graph;
    }

    int MaterialGraphEditor::CollectParams(glm::vec4* out, int maxCount) const
    {
        int slot = 0;
        for (const auto& n : m_Nodes)
        {
            if (n.Type != EMaterialNodeType::ParamScalar && n.Type != EMaterialNodeType::ParamColor)
                continue;
            if (slot >= maxCount || slot >= MAX_PARAMS)
                break;
            out[slot++] = n.Constant;
        }
        return slot;
    }

    bool MaterialGraphEditor::Save(const std::string& path) const
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        std::ofstream out(path);
        if (!out)
        {
            EE_CORE_ERROR("Material graph: failed to open '{}' for writing.", path)
            return false;
        }

        out << "{\n";
        out << "  \"version\": 1,\n";
        out << "  \"targetMaterial\": " << m_TargetMaterial << ",\n";
        out << "  \"nextId\": " << m_NextId << ",\n";
        out << "  \"channels\": [";
        for (int i = 0; i < 5; ++i) out << (i ? ", " : "") << m_Channels[i];
        out << "],\n";
        out << "  \"nodes\": [\n";
        for (size_t k = 0; k < m_Nodes.size(); ++k)
        {
            const SNode& n = m_Nodes[k];
            out << "    { "
                << "\"id\": " << n.Id << ", "
                << "\"type\": " << (int)n.Type << ", "
                << "\"outputType\": " << (int)n.OutputType << ", "
                << "\"pos\": [" << n.Pos.x << ", " << n.Pos.y << "], "
                << "\"constant\": [" << n.Constant.x << ", " << n.Constant.y << ", "
                << n.Constant.z << ", " << n.Constant.w << "], "
                << "\"param\": \"" << n.Param << "\", "
                << "\"texSlot\": " << n.TexSlot << ", "
                << "\"inputCount\": " << n.InputCount << ", "
                << "\"inputs\": [" << n.Inputs[0] << ", " << n.Inputs[1] << ", " << n.Inputs[2] << "] }"
                << (k + 1 < m_Nodes.size() ? "," : "") << "\n";
        }
        out << "  ]\n}\n";

        EE_CORE_INFO("Material graph saved to '{}'.", path)
        return true;
    }

    bool MaterialGraphEditor::Load(const std::string& path)
    {
        auto json = simdjson::padded_string::load(path);
        if (json.error())
        {
            EE_CORE_ERROR("Material graph: failed to open '{}': {}", path, simdjson::error_message(json.error()))
            return false;
        }

        simdjson::dom::parser parser;
        simdjson::dom::element doc;
        if (const auto err = parser.parse(json.value()).get(doc))
        {
            EE_CORE_ERROR("Material graph: failed to parse '{}': {}", path, simdjson::error_message(err))
            return false;
        }

        // Reads a number tolerant of integer vs floating-point tokens.
        const auto num = [](simdjson::dom::element e, double def) -> double
        {
            double d;
            if (e.get(d) == simdjson::SUCCESS) return d;
            int64_t i;
            if (e.get(i) == simdjson::SUCCESS) return (double)i;
            return def;
        };

        // Only commit to the parsed state once we know the document is well-formed.
        std::vector<SNode> nodes;
        int channels[5] = { -1, -1, -1, -1, -1 };
        int targetMaterial = m_TargetMaterial;
        int nextId = 1;

        int64_t iv;
        if (doc["targetMaterial"].get(iv) == simdjson::SUCCESS) targetMaterial = (int)iv;
        if (doc["nextId"].get(iv) == simdjson::SUCCESS) nextId = (int)iv;

        simdjson::dom::array chans;
        if (doc["channels"].get(chans) == simdjson::SUCCESS)
        {
            int i = 0;
            for (auto c : chans)
            {
                if (i < 5) channels[i] = (int)num(c, -1.0);
                ++i;
            }
        }

        simdjson::dom::array arr;
        if (doc["nodes"].get(arr) == simdjson::SUCCESS)
        {
            for (auto e : arr)
            {
                SNode node;
                if (e["id"].get(iv) == simdjson::SUCCESS) node.Id = (int)iv;
                if (e["type"].get(iv) == simdjson::SUCCESS) node.Type = (EMaterialNodeType)iv;
                if (e["outputType"].get(iv) == simdjson::SUCCESS) node.OutputType = (EGraphValueType)iv;
                if (e["texSlot"].get(iv) == simdjson::SUCCESS) node.TexSlot = (int)iv;
                if (e["inputCount"].get(iv) == simdjson::SUCCESS) node.InputCount = (int)iv;

                simdjson::dom::array pos;
                if (e["pos"].get(pos) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : pos) { if (i == 0) node.Pos.x = (float)num(v, 0.0); else if (i == 1) node.Pos.y = (float)num(v, 0.0); ++i; }
                }
                simdjson::dom::array con;
                if (e["constant"].get(con) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : con) { if (i < 4) (&node.Constant.x)[i] = (float)num(v, 0.0); ++i; }
                }
                std::string_view sv;
                if (e["param"].get(sv) == simdjson::SUCCESS)
                {
                    const size_t len = std::min(sv.size(), sizeof(node.Param) - 1);
                    std::memcpy(node.Param, sv.data(), len);
                    node.Param[len] = '\0';
                }
                simdjson::dom::array ins;
                if (e["inputs"].get(ins) == simdjson::SUCCESS)
                {
                    int i = 0;
                    for (auto v : ins) { if (i < 3) node.Inputs[i] = (int)num(v, -1.0); ++i; }
                }
                nodes.push_back(node);
            }
        }

        m_Nodes = std::move(nodes);
        for (int i = 0; i < 5; ++i) m_Channels[i] = channels[i];
        m_TargetMaterial = targetMaterial;
        m_NextId = nextId > 1 ? nextId : 1;
        m_LinkFrom = -1;

        EE_CORE_INFO("Material graph loaded from '{}' ({} nodes).", path, m_Nodes.size())
        return true;
    }
}
