#include "epch.h"
#include "MaterialGraphEditor.h"

#include <imgui.h>

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
                case EMaterialNodeType::Parameter:     return "Parameter";
                case EMaterialNodeType::TextureSample: return "Texture";
                case EMaterialNodeType::TexCoord:      return "TexCoord";
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
                case EMaterialNodeType::Saturate: return 1;
                default: return 0;
            }
        }

        EGraphValueType OutputTypeFor(EMaterialNodeType t)
        {
            switch (t)
            {
                case EMaterialNodeType::Fresnel:
                case EMaterialNodeType::Dot:           return EGraphValueType::Float;
                case EMaterialNodeType::TexCoord:      return EGraphValueType::Float2;
                case EMaterialNodeType::TextureSample: return EGraphValueType::Float3;
                default:                               return EGraphValueType::Float4;
            }
        }

        // The guarded HLSL sample expression for a texture slot (0..4). Falls back to
        // white when the material has no texture bound in that slot.
        std::string TextureSlotExpr(int slot)
        {
            const char* idx =
                slot == 0 ? "mat.TexIndex0.x" :
                slot == 1 ? "mat.TexIndex0.y" :
                slot == 2 ? "mat.TexIndex0.z" :
                slot == 3 ? "mat.TexIndex0.w" :
                            "mat.TexIndex1.x";
            return std::string("(") + idx + " == 0xffffffffu ? float3(1.0, 1.0, 1.0) : SampleTex("
                + idx + ", input.TexCoord))";
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
        m_Nodes.push_back(node);
        return node.Id;
    }

    const MaterialGraphEditor::SNode* MaterialGraphEditor::Find(int id) const
    {
        for (const auto& n : m_Nodes)
            if (n.Id == id) return &n;
        return nullptr;
    }

    bool MaterialGraphEditor::Draw()
    {
        bool apply = false;
        ImGui::Begin("Node Graph Editor");

        auto addButton = [&](const char* label, EMaterialNodeType type)
        {
            if (ImGui::Button(label)) AddNode(type, { 40.0f, 40.0f });
            ImGui::SameLine();
        };

        ImGui::TextUnformatted("Inputs:"); ImGui::SameLine();
        addButton("Constant", EMaterialNodeType::Constant);
        addButton("Parameter", EMaterialNodeType::Parameter);
        addButton("Texture", EMaterialNodeType::TextureSample);
        addButton("TexCoord", EMaterialNodeType::TexCoord);
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

        if (ImGui::Button("Apply")) apply = true;
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
            if (ImGui::IsItemActive())
            {
                node.Pos.x += io.MouseDelta.x;
                node.Pos.y += io.MouseDelta.y;
                p = ImVec2(origin.x + node.Pos.x, origin.y + node.Pos.y);
            }

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
            else if (node.Type == EMaterialNodeType::Parameter)
                ImGui::InputText("##p", node.Param, sizeof(node.Param));
            else if (node.Type == EMaterialNodeType::TextureSample)
                ImGui::Combo("##t", &node.TexSlot, kTextureSlots, IM_ARRAYSIZE(kTextureSlots));
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
        const ImVec2 omax = ImVec2(op.x + 140.0f, op.y + 20.0f + 4 * 20.0f + 6.0f);
        dl->AddRectFilled(omin, omax, IM_COL32(52, 42, 42, 235), 4.0f);
        dl->AddRectFilled(omin, ImVec2(omax.x, omin.y + 20.0f), IM_COL32(120, 80, 80, 255), 4.0f);
        dl->AddRect(omin, omax, IM_COL32(140, 100, 100, 255), 4.0f);
        dl->AddText(ImVec2(omin.x + 8.0f, omin.y + 3.0f), IM_COL32_WHITE, "Output");
        ImGui::PushID(-99);
        std::unordered_map<int, ImVec2> chPins;
        for (int ch = 0; ch < 4; ++ch)
        {
            const ImVec2 pinPos = ImVec2(omin.x, omin.y + 30.0f + ch * 20.0f);
            ImGui::PushID(ch);
            const auto [active, hovered] = pinButton("ch", pinPos);
            ImGui::PopID();
            if (hovered)
            {
                hovKind = 1; hovId = ch; hovSlot = 0;
            }
            const bool connected = m_Channels[ch] >= 0;
            dl->AddCircleFilled(pinPos, PIN_R, connected ? IM_COL32(140, 180, 240, 255) : IM_COL32(90, 110, 150, 255));
            dl->AddText(ImVec2(omin.x + 12.0f, pinPos.y - 7.0f), IM_COL32_WHITE, ChannelName(ch));
            chPins[ch] = pinPos;
        }
        ImGui::PopID();

        // --- Links ---
        auto bezier = [&](const ImVec2& a, const ImVec2& b, ImU32 col)
        {
            dl->AddBezierCubic(a, ImVec2(a.x + 50.0f, a.y), ImVec2(b.x - 50.0f, b.y), b, col, 2.5f);
        };
        for (const auto& node : m_Nodes)
            for (int i = 0; i < node.InputCount; ++i)
                if (node.Inputs[i] >= 0 && outPins.count(node.Inputs[i]))
                    bezier(outPins[node.Inputs[i]], inPinPos[((long long)node.Id << 8) | i], IM_COL32(200, 200, 210, 200));
        for (int ch = 0; ch < 4; ++ch)
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
        return apply;
    }

    MaterialGraph MaterialGraphEditor::Build() const
    {
        MaterialGraph graph;
        std::unordered_map<int, uint32_t> idMap;

        for (const auto& n : m_Nodes)
        {
            SMaterialNode gn;
            gn.Type = n.Type;
            gn.OutputType = n.OutputType;
            gn.ConstantValue = n.Constant;
            gn.ParameterName = n.Param;
            gn.Inputs.assign(n.InputCount, -1);
            if (n.Type == EMaterialNodeType::TextureSample)
                gn.TextureExpression = TextureSlotExpr(n.TexSlot);
            idMap[n.Id] = graph.AddNode(gn);
        }

        for (const auto& n : m_Nodes)
            for (int i = 0; i < n.InputCount; ++i)
                if (n.Inputs[i] >= 0 && idMap.count(n.Inputs[i]))
                    graph.Connect(idMap[n.Inputs[i]], idMap[n.Id], (uint32_t)i);

        for (int ch = 0; ch < 4; ++ch)
            if (m_Channels[ch] >= 0 && idMap.count(m_Channels[ch]))
                graph.SetChannel((EMaterialChannel)ch, idMap[m_Channels[ch]]);

        return graph;
    }
}
