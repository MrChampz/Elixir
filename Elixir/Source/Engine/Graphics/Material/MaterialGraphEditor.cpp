#include "epch.h"
#include "MaterialGraphEditor.h"

#include <imgui.h>
#include <simdjson.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>

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
                case EMaterialNodeType::Vector:        return "Vector";
                case EMaterialNodeType::Bool:          return "Bool";
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
                case EMaterialNodeType::Custom:        return "Custom HLSL";
                case EMaterialNodeType::FunctionInput: return "Fn Input";
                case EMaterialNodeType::FunctionCall:  return "Fn Call";
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
                case EMaterialNodeType::Custom:
                case EMaterialNodeType::FunctionCall: return 3; // a, b, c / fn inputs
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
                case EMaterialNodeType::Bool:
                case EMaterialNodeType::ParamScalar:
                case EMaterialNodeType::Checker:
                case EMaterialNodeType::Noise:
                case EMaterialNodeType::Time:          return EGraphValueType::Float;
                case EMaterialNodeType::TexCoord:
                case EMaterialNodeType::Panner:        return EGraphValueType::Float2;
                case EMaterialNodeType::TextureSample:
                case EMaterialNodeType::Vector:
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
                case 5: return "World Pos Offset";
                case 6: return "Opacity";
                case 7: return "Subsurface Color";
                case 8: return "Clear Coat";
                case 9: return "Clear Coat Rough";
                case 10: return "Sheen";
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
        m_Document.Nodes[0].Constant = { 1.0f, 0.5f, 0.1f, 1.0f };
        m_Document.Nodes[2].Inputs[0] = c;
        m_Document.Nodes[2].Inputs[1] = p;
        m_Document.Channels[0] = m;

        m_LastSig = Signature(); // seed graph isn't "dirty" until the user edits it
        m_LastStructHash = m_Document.StructHash();
        m_Committed = m_Document;
        m_CommittedHash = m_Committed.Hash();
    }

    int MaterialGraphEditor::AddNode(EMaterialNodeType type, const glm::vec2& pos)
    {
        SNode node;
        node.Id = m_Document.NextId++;
        node.DiagnosticId = node.Id;
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
        if (type == EMaterialNodeType::FunctionInput)
            std::strncpy(node.Param, "In", sizeof(node.Param) - 1);
        if (type == EMaterialNodeType::FunctionCall)
            std::strncpy(node.Param, "myfunc", sizeof(node.Param) - 1);
        m_Document.Nodes.push_back(node);
        return node.Id;
    }

    void MaterialGraphEditor::DeleteNode(int id)
    {
        m_Document.Nodes.erase(
            std::remove_if(m_Document.Nodes.begin(), m_Document.Nodes.end(), [id](const SNode& n) { return n.Id == id; }),
            m_Document.Nodes.end());

        // Drop any wires that fed from the deleted node.
        for (auto& n : m_Document.Nodes)
            for (int& src : n.Inputs)
                if (src == id) src = -1;
        for (int& ch : m_Document.Channels)
            if (ch == id) ch = -1;
        if (m_LinkFrom == id)
            m_LinkFrom = -1;
        m_Selected.erase(std::remove(m_Selected.begin(), m_Selected.end(), id), m_Selected.end());
    }

    const MaterialGraphEditor::SNode* MaterialGraphEditor::Find(int id) const
    {
        for (const auto& n : m_Document.Nodes)
            if (n.Id == id) return &n;
        return nullptr;
    }

    bool MaterialGraphEditor::Draw(int materialCount)
    {
        bool apply = false;
        m_SavedThisFrame = false;
        m_LoadedThisFrame = false;
        ImGui::Begin("Node Graph Editor");

        m_Diagnostics = Build().Validate().Diagnostics;
        int errorCount = 0, warningCount = 0;
        for (const auto& diagnostic : m_Diagnostics)
        {
            if (diagnostic.Severity == EMaterialDiagnosticSeverity::Error) ++errorCount;
            else ++warningCount;
        }
        const bool hasErrors = errorCount > 0;
        const auto nodeSeverity = [&](uint32_t nodeId)
        {
            int severity = -1; // -1=none, 0=warning, 1=error
            for (const auto& diagnostic : m_Diagnostics)
                if (diagnostic.NodeId == nodeId)
                    severity = std::max(severity,
                        diagnostic.Severity == EMaterialDiagnosticSeverity::Error ? 1 : 0);
            return severity;
        };
        const auto diagnosticTooltip = [&](uint32_t nodeId)
        {
            bool any = false;
            for (const auto& diagnostic : m_Diagnostics)
                any |= diagnostic.NodeId == nodeId;
            if (!any)
                return;
            ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 0.0f), ImVec2(520.0f, FLT_MAX));
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 480.0f);
            for (const auto& diagnostic : m_Diagnostics)
            {
                if (diagnostic.NodeId != nodeId)
                    continue;
                const ImVec4 color = diagnostic.Severity == EMaterialDiagnosticSeverity::Error
                    ? ImVec4(1.0f, 0.35f, 0.30f, 1.0f) : ImVec4(1.0f, 0.72f, 0.25f, 1.0f);
                ImGui::TextColored(color, "%s: %s",
                    diagnostic.Severity == EMaterialDiagnosticSeverity::Error ? "Error" : "Warning",
                    diagnostic.Message.c_str());
            }
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        };

        const int maxMaterial = materialCount > 0 ? materialCount - 1 : 0;
        if (m_Document.TargetMaterial > maxMaterial) m_Document.TargetMaterial = maxMaterial;
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderInt("Target material", &m_Document.TargetMaterial, 0, maxMaterial);

        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo("Blend", &m_Document.BlendMode, "Opaque\0Masked\0Translucent\0Additive\0");
        if (m_Document.BlendMode == 1) // Masked
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderFloat("Cutoff", &m_Document.AlphaCutoff, 0.0f, 1.0f, "%.2f");
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130.0f);
        ImGui::Combo("Shading", &m_Document.ShadingModel, "Default Lit\0Unlit\0Subsurface\0Clear Coat\0Cloth\0");

        // Save / load the parent Material: this graph and its defaults. Instances of it
        // are named and saved from the Material Editor. The host does the IO once
        // Draw() returns.
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##file", m_FileName, sizeof(m_FileName));
        ImGui::SameLine();
        if (ImGui::Button("Save")) m_SavedThisFrame = true;
        ImGui::SameLine();
        if (ImGui::Button("Load")) m_LoadedThisFrame = true;
        ImGui::SameLine();
        ImGui::TextDisabled(".material.json");
        ImGui::Separator();

        auto addButton = [&](const char* label, EMaterialNodeType type)
        {
            if (ImGui::Button(label)) AddNode(type, { 40.0f, 40.0f });
            ImGui::SameLine();
        };

        ImGui::TextUnformatted("Inputs:"); ImGui::SameLine();
        addButton("Constant", EMaterialNodeType::Constant);
        addButton("Scalar", EMaterialNodeType::Scalar);
        addButton("Vector", EMaterialNodeType::Vector);
        addButton("Bool", EMaterialNodeType::Bool);
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
        addButton("Custom", EMaterialNodeType::Custom);
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

        ImGui::TextUnformatted("Func:  "); ImGui::SameLine();
        addButton("Fn Input", EMaterialNodeType::FunctionInput);
        addButton("Fn Call", EMaterialNodeType::FunctionCall);
        ImGui::NewLine();

        ImGui::BeginDisabled(hasErrors);
        if (ImGui::Button("Apply")) apply = true;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Checkbox("Live", &m_LivePreview);
        ImGui::SameLine();
        if (ImGui::Button("+Comment"))
            m_Document.Comments.push_back({ { -m_Pan.x + 20.0f, -m_Pan.y + 20.0f }, { 220.0f, 140.0f }, "Comment" });
        ImGui::SameLine();
        ImGui::TextDisabled("(right-click: node = delete, pin = disconnect)");
        ImGui::SameLine();
        if (hasErrors)
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "%d error%s", errorCount, errorCount == 1 ? "" : "s");
        else if (warningCount > 0)
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%d warning%s", warningCount, warningCount == 1 ? "" : "s");
        else
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Valid");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 0.0f), ImVec2(520.0f, FLT_MAX));
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 480.0f);
            for (const auto& diagnostic : m_Diagnostics)
                ImGui::Text("%s%s", diagnostic.Severity == EMaterialDiagnosticSeverity::Error ? "Error: " : "Warning: ",
                    diagnostic.Message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::Separator();

        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float NODE_W = 150.0f;
        const float PIN_R = 6.0f;

        // Background catcher: consumes drags on empty canvas (so they pan instead of
        // moving the window) while SetNextItemAllowOverlap lets the node/pin buttons
        // submitted on top still take their interaction.
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorScreenPos(origin);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("canvas", ImVec2(avail.x > 1.0f ? avail.x : 1.0f, avail.y > 1.0f ? avail.y : 1.0f),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
        if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
        {
            m_Pan.x += io.MouseDelta.x;
            m_Pan.y += io.MouseDelta.y;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !io.KeyShift)
            m_Selected.clear(); // click empty canvas to deselect

        // Canvas origin with the pan offset applied (all node/pin/link coords use it).
        const ImVec2 base = ImVec2(origin.x + m_Pan.x, origin.y + m_Pan.y);

        // Clip the canvas so a panned graph doesn't draw over the toolbar. Use the
        // ImGui-managed clip (not the raw draw list) so it composes correctly with the
        // widgets submitted inside it (e.g. the comment text field).
        ImGui::PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);

        std::unordered_map<int, ImVec2> outPins;   // node id -> output pin pos
        std::unordered_map<long long, ImVec2> inPinPos; // encoded target -> input pin pos

        // Hovered input pin this frame (for connect-on-release). Allowed to register
        // even while the output pin is the active (dragged) item.
        int hovKind = -1, hovId = -1, hovSlot = -1;

        // Node scheduled for deletion this frame (processed after drawing so we don't
        // mutate m_Document.Nodes mid-iteration).
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

        // --- Comment boxes (behind the nodes) ---
        int deleteComment = -1;
        for (size_t ci = 0; ci < m_Document.Comments.size(); ++ci)
        {
            auto& com = m_Document.Comments[ci];
            const ImVec2 cmin = ImVec2(base.x + com.Pos.x, base.y + com.Pos.y);
            const ImVec2 cmax = ImVec2(cmin.x + com.Size.x, cmin.y + com.Size.y);
            dl->AddRectFilled(cmin, cmax, IM_COL32(70, 100, 80, 45), 4.0f);
            dl->AddRect(cmin, cmax, IM_COL32(120, 180, 140, 160), 4.0f);
            dl->AddRectFilled(cmin, ImVec2(cmax.x, cmin.y + 20.0f), IM_COL32(70, 100, 80, 150), 4.0f);

            ImGui::PushID((const void*)&com); // pointer id can't collide with node ids

            // The label owns the left side of the title bar. Keep the drag zone to
            // its right so InputText never competes with cdrag for the active ID.
            constexpr float LABEL_INSET = 4.0f;
            const float labelWidth = com.Size.x * 0.7f;
            const float dragX = cmin.x + LABEL_INSET + labelWidth;
            const float dragWidth = std::max(1.0f, com.Size.x - LABEL_INSET - labelWidth);

            // Title bar: drag to move, right-click to delete.
            ImGui::SetCursorScreenPos(ImVec2(dragX, cmin.y));
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("cdrag", ImVec2(dragWidth, 20.0f));
            if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                com.Pos.x += io.MouseDelta.x;
                com.Pos.y += io.MouseDelta.y;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                deleteComment = (int)ci;

            // Editable label (left part of the title; the rest stays a drag zone).
            ImGui::SetCursorScreenPos(ImVec2(cmin.x + LABEL_INSET, cmin.y + 1.0f));
            ImGui::SetNextItemWidth(labelWidth);
            ImGui::InputText("##ctext", com.Text, sizeof(com.Text));

            // Resize grip (bottom-right).
            ImGui::SetCursorScreenPos(ImVec2(cmax.x - 14.0f, cmax.y - 14.0f));
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("cresize", ImVec2(14.0f, 14.0f));
            if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                com.Size.x = std::max(120.0f, com.Size.x + io.MouseDelta.x);
                com.Size.y = std::max(50.0f, com.Size.y + io.MouseDelta.y);
            }
            dl->AddTriangleFilled(ImVec2(cmax.x - 12.0f, cmax.y - 2.0f), ImVec2(cmax.x - 2.0f, cmax.y - 12.0f),
                ImVec2(cmax.x - 2.0f, cmax.y - 2.0f), IM_COL32(140, 200, 160, 200));

            ImGui::PopID();
        }
        if (deleteComment >= 0)
            m_Document.Comments.erase(m_Document.Comments.begin() + deleteComment);

        // --- Nodes ---
        for (auto& node : m_Document.Nodes)
        {
            ImVec2 p = ImVec2(base.x + node.Pos.x, base.y + node.Pos.y);
            ImGui::PushID(node.Id);

            // Header acts as the drag handle.
            ImGui::SetCursorScreenPos(p);
            ImGui::InvisibleButton("hdr", ImVec2(NODE_W, 20.0f));
            const bool headerHovered = ImGui::IsItemHovered();
            if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                node.Pos.x += io.MouseDelta.x;
                node.Pos.y += io.MouseDelta.y;
                p = ImVec2(base.x + node.Pos.x, base.y + node.Pos.y);
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                deleteId = node.Id;

            // Selection: click selects (shift-click toggles) for copy/paste.
            const bool selected = std::find(m_Selected.begin(), m_Selected.end(), node.Id) != m_Selected.end();
            if (ImGui::IsItemActivated())
            {
                if (io.KeyShift)
                {
                    if (selected) m_Selected.erase(std::remove(m_Selected.begin(), m_Selected.end(), node.Id), m_Selected.end());
                    else m_Selected.push_back(node.Id);
                }
                else if (!selected)
                {
                    m_Selected.clear();
                    m_Selected.push_back(node.Id);
                }
            }

            // Nodes with two stacked body widgets (text + combo) need room for both.
            const int widgetRows =
                (node.Type == EMaterialNodeType::Custom || node.Type == EMaterialNodeType::FunctionInput) ? 2 : 1;
            const int rows = std::max({ node.InputCount, widgetRows, 1 });
            const float bodyH = 24.0f + rows * 16.0f;
            const ImVec2 rmin = p;
            const ImVec2 rmax = ImVec2(p.x + NODE_W, p.y + 20.0f + bodyH);

            dl->AddRectFilled(rmin, rmax, IM_COL32(42, 42, 52, 235), 4.0f);
            dl->AddRectFilled(rmin, ImVec2(rmax.x, rmin.y + 20.0f), IM_COL32(70, 70, 95, 255), 4.0f);
            const int severity = nodeSeverity((uint32_t)node.Id);
            const ImU32 outline = severity == 1 ? IM_COL32(245, 70, 65, 255)
                : severity == 0 ? IM_COL32(245, 165, 55, 255)
                : selected ? IM_COL32(255, 210, 90, 255) : IM_COL32(100, 100, 125, 255);
            dl->AddRect(rmin, rmax, outline, 4.0f, 0, severity >= 0 || selected ? 2.0f : 1.0f);
            dl->AddText(ImVec2(rmin.x + 8.0f, rmin.y + 3.0f), IM_COL32_WHITE, NodeName(node.Type));
            if (headerHovered && severity >= 0)
                diagnosticTooltip((uint32_t)node.Id);

            // Value editors.
            ImGui::SetCursorScreenPos(ImVec2(rmin.x + 8.0f, rmin.y + 24.0f));
            ImGui::PushItemWidth(NODE_W - 16.0f);
            if (node.Type == EMaterialNodeType::Constant)
                ImGui::ColorEdit4("##v", &node.Constant.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            else if (node.Type == EMaterialNodeType::Scalar)
                ImGui::DragFloat("##f", &node.Constant.x, 0.05f, -100.0f, 100.0f, "%.3f");
            else if (node.Type == EMaterialNodeType::Bool)
            {
                bool b = node.Constant.x >= 0.5f;
                if (ImGui::Checkbox("on", &b)) node.Constant.x = b ? 1.0f : 0.0f;
            }
            else if (node.Type == EMaterialNodeType::Vector)
                ImGui::DragFloat3("##v3", &node.Constant.x, 0.01f, -100.0f, 100.0f, "%.3f");
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
            else if (node.Type == EMaterialNodeType::Custom)
            {
                ImGui::InputText("##code", node.Code, sizeof(node.Code));
                // Stacked widgets: reset X to the node (ImGui advances to the window margin).
                ImGui::SetCursorScreenPos(ImVec2(rmin.x + 8.0f, ImGui::GetCursorScreenPos().y));
                int ot = (int)node.OutputType;
                if (ImGui::Combo("##ot", &ot, "float\0float2\0float3\0float4\0"))
                    node.OutputType = (EGraphValueType)ot;
            }
            else if (node.Type == EMaterialNodeType::FunctionInput)
            {
                ImGui::InputText("##fn", node.Param, sizeof(node.Param));
                ImGui::SetCursorScreenPos(ImVec2(rmin.x + 8.0f, ImGui::GetCursorScreenPos().y));
                ImGui::Combo("##sl", &node.TexSlot, "slot 0\0slot 1\0slot 2\0");
            }
            else if (node.Type == EMaterialNodeType::FunctionCall)
                ImGui::InputText("##file", node.Param, sizeof(node.Param)); // function file name
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
        // The surface channels are always shown; the current shading model adds its
        // own inputs (channels 7..10) so the Output node stays uncluttered.
        std::vector<int> vis = { 0, 1, 2, 3, 4, 5, 6 };
        if (m_Document.ShadingModel == 2) vis.push_back(7);                               // Subsurface Color
        else if (m_Document.ShadingModel == 3) { vis.push_back(8); vis.push_back(9); }    // Clear Coat (+ roughness)
        else if (m_Document.ShadingModel == 4) vis.push_back(10);                         // Sheen

        const ImVec2 op = ImVec2(base.x + 520.0f, base.y + 40.0f);
        const ImVec2 omin = op;
        const ImVec2 omax = ImVec2(op.x + 170.0f, op.y + 20.0f + (float)vis.size() * 20.0f + 6.0f);
        dl->AddRectFilled(omin, omax, IM_COL32(52, 42, 42, 235), 4.0f);
        dl->AddRectFilled(omin, ImVec2(omax.x, omin.y + 20.0f), IM_COL32(120, 80, 80, 255), 4.0f);
        const int outputSeverity = nodeSeverity(0);
        const ImU32 outputOutline = outputSeverity == 1 ? IM_COL32(245, 70, 65, 255)
            : outputSeverity == 0 ? IM_COL32(245, 165, 55, 255) : IM_COL32(140, 100, 100, 255);
        dl->AddRect(omin, omax, outputOutline, 4.0f, 0, outputSeverity >= 0 ? 2.0f : 1.0f);
        dl->AddText(ImVec2(omin.x + 8.0f, omin.y + 3.0f), IM_COL32_WHITE, "Output");
        ImGui::PushID(-99);
        std::unordered_map<int, ImVec2> chPins;
        for (size_t row = 0; row < vis.size(); ++row)
        {
            const int ch = vis[row];
            const ImVec2 pinPos = ImVec2(omin.x, omin.y + 30.0f + row * 20.0f);
            ImGui::PushID(ch);
            const auto [active, hovered] = pinButton("ch", pinPos);
            ImGui::PopID();
            if (hovered)
            {
                hovKind = 1; hovId = ch; hovSlot = 0;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    m_Document.Channels[ch] = -1;
            }
            const bool connected = m_Document.Channels[ch] >= 0;
            dl->AddCircleFilled(pinPos, PIN_R, connected ? IM_COL32(140, 180, 240, 255) : IM_COL32(90, 110, 150, 255));
            dl->AddText(ImVec2(omin.x + 12.0f, pinPos.y - 7.0f), IM_COL32_WHITE, ChannelName(ch));
            chPins[ch] = pinPos;
        }
        ImGui::PopID();
        if (outputSeverity >= 0 && ImGui::IsMouseHoveringRect(omin, omax))
            diagnosticTooltip(0);

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
        for (const auto& node : m_Document.Nodes)
            for (int i = 0; i < node.InputCount; ++i)
                if (node.Inputs[i] >= 0 && outPins.count(node.Inputs[i]))
                    bezier(outPins[node.Inputs[i]], inPinPos[((long long)node.Id << 8) | i], IM_COL32(200, 200, 210, 200));
        for (const int ch : vis)
            if (m_Document.Channels[ch] >= 0 && outPins.count(m_Document.Channels[ch]))
                bezier(outPins[m_Document.Channels[ch]], chPins[ch], IM_COL32(220, 190, 90, 220));

        // --- Drag-to-connect ---
        if (m_LinkFrom >= 0 && outPins.count(m_LinkFrom))
            bezier(outPins[m_LinkFrom], io.MousePos, IM_COL32(255, 230, 120, 255));

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_LinkFrom >= 0)
        {
            if (hovKind == 0)
            {
                for (auto& n : m_Document.Nodes)
                    if (n.Id == hovId && hovSlot < n.InputCount)
                        n.Inputs[hovSlot] = m_LinkFrom;
            }
            else if (hovKind == 1)
            {
                m_Document.Channels[hovId] = m_LinkFrom;
            }
            m_LinkFrom = -1;
        }

        // --- Undo / redo ---
        bool interacting = ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left)
            || ImGui::IsMouseDown(ImGuiMouseButton_Middle) || m_LinkFrom >= 0;
        if (ImGui::IsWindowFocused())
        {
            const bool mod = io.KeyCtrl || io.KeySuper; // Ctrl / Cmd
            const bool undo = mod && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z);
            const bool redo = mod && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)));

            // Copy selected nodes; paste duplicates them (new ids, offset, internal
            // wiring remapped, external inputs kept).
            if (mod && ImGui::IsKeyPressed(ImGuiKey_C) && !m_Selected.empty())
            {
                m_Clipboard.clear();
                for (const int id : m_Selected)
                    if (const SNode* n = Find(id)) m_Clipboard.push_back(*n);
            }
            if (mod && ImGui::IsKeyPressed(ImGuiKey_V) && !m_Clipboard.empty())
            {
                std::unordered_map<int, int> idMap;
                for (const auto& src : m_Clipboard) idMap[src.Id] = m_Document.NextId++;
                m_Selected.clear();
                for (const auto& src : m_Clipboard)
                {
                    SNode c = src;
                    c.Id = idMap[src.Id];
                    c.DiagnosticId = c.Id;
                    c.FunctionStack.clear();
                    c.Pos.x += 30.0f;
                    c.Pos.y += 30.0f;
                    for (int i = 0; i < 3; ++i)
                        if (c.Inputs[i] >= 0 && idMap.count(c.Inputs[i]))
                            c.Inputs[i] = idMap[c.Inputs[i]]; // internal link -> the copy
                    m_Document.Nodes.push_back(c);
                    m_Selected.push_back(c.Id);
                }
            }
            if (undo && !m_Undo.empty())
            {
                m_Redo.push_back(m_Committed);
                m_Committed = m_Undo.back(); m_Undo.pop_back();
                Restore(m_Committed); m_CommittedHash = m_Committed.Hash();
                interacting = true;
            }
            else if (redo && !m_Redo.empty())
            {
                m_Undo.push_back(m_Committed);
                m_Committed = m_Redo.back(); m_Redo.pop_back();
                Restore(m_Committed); m_CommittedHash = m_Committed.Hash();
                interacting = true;
            }
        }
        CommitUndoIfSettled(interacting);

        ImGui::PopClipRect(); // end canvas clip

        ImGui::End();

        // Exposed-parameter panel (own window): the editor mirrors values to the target
        // MaterialInstance, which the renderer uploads without recompiling.
        ImGui::Begin("Material Parameters");
        ImGui::TextDisabled("Live -- no recompile. Includes params inside functions.");
        ImGui::Separator();
        {
            // List params across the expanded graph (top-level + inside functions);
            // Edits go to a local UI map keyed by name and are applied to the instance
            // immediately after Draw() by the host application.
            std::vector<SNode> pnodes;
            int pchannels[11];
            m_Document.Expand(pnodes, pchannels);
            int slot = 0;
            for (const auto& node : pnodes)
            {
                if (node.Type != EMaterialNodeType::ParamScalar && node.Type != EMaterialNodeType::ParamColor)
                    continue;
                if (slot >= MAX_PARAMS) break;

                const auto it = m_Document.Overrides.find(node.Param);
                glm::vec4 value = it != m_Document.Overrides.end() ? it->second : node.Constant;

                ImGui::PushID(slot);
                ImGui::SetNextItemWidth(200.0f);
                bool changed;
                if (node.Type == EMaterialNodeType::ParamScalar)
                    changed = ImGui::DragFloat(node.Param, &value.x, 0.01f, -10.0f, 10.0f, "%.3f");
                else
                    changed = ImGui::ColorEdit4(node.Param, &value.x, ImGuiColorEditFlags_AlphaBar);
                if (changed)
                    m_Document.Overrides[node.Param] = value;
                ImGui::PopID();
                ++slot;
            }
            if (slot == 0)
                ImGui::TextDisabled("(no exposed parameters)");
        }
        ImGui::End();

        // Live preview: recompile a short debounce after the last graph change, so a
        // drag doesn't recompile every frame. Values that stream live (params) are
        // excluded from the signature.
        constexpr double DEBOUNCE = 0.3;
        // Only run the expensive HLSL-generating Signature() when the cheap structural
        // hash says the graph might have changed -- otherwise it ran every frame and
        // could push the UI thread over the frame budget (scene hitch while typing).
        const size_t structHash = m_Document.StructHash();
        if (structHash != m_LastStructHash)
        {
            m_LastStructHash = structHash;
            const size_t sig = Signature();
            if (sig != m_LastSig)
            {
                m_LastSig = sig;
                m_DirtySince = ImGui::GetTime();
            }
        }
        // Only fire once the change settles AND nothing is being interacted with, so a
        // recompile never lands mid-drag (e.g. while editing/resizing a comment).
        if (m_LivePreview && m_DirtySince >= 0.0 && ImGui::GetTime() - m_DirtySince > DEBOUNCE && !interacting)
        {
            m_DirtySince = -1.0;
            apply = true;
        }

        if (apply)
        {
            SMaterialGraphValidation validation = Build().Validate();
            m_Diagnostics = validation.Diagnostics;
            if (validation.HasErrors())
                apply = false;
        }

        return apply;
    }

    void MaterialGraphEditor::SyncParametersFrom(const MaterialInstance& instance)
    {
        const Ref<Material>& material = instance.GetParent();
        if (!material || !material->HasGraph())
            return;

        for (const auto& parameter : material->GetGraphParameters())
        {
            const SMaterialParam* value = instance.GetParameter(parameter.Name);
            if (!value || value->Type != parameter.Type)
                continue;
            if (parameter.Type == SMaterialParam::EType::Scalar)
                m_Document.Overrides[parameter.Name] = glm::vec4(value->Scalar, 0.0f, 0.0f, 0.0f);
            else if (parameter.Type == SMaterialParam::EType::Vector)
                m_Document.Overrides[parameter.Name] = value->Vector;
        }
    }

    size_t MaterialGraphEditor::Signature() const
    {
        // Hash the actual generated HLSL, so the signature changes only when the
        // compiled result would: connecting/disconnecting, rewiring a channel, or
        // editing a *reachable* baked constant. Disconnected nodes and exposed-param
        // values (which emit no literal) don't affect it -> no needless recompile.
        const MaterialGraph g = Build();
        const SMaterialGraphValidation validation = g.Validate();
        if (validation.HasErrors())
        {
            size_t h = 1469598103934665603ull;
            for (const auto& diagnostic : validation.Diagnostics)
            {
                h ^= diagnostic.NodeId; h *= 1099511628211ull;
                h ^= std::hash<std::string>{}(diagnostic.Message); h *= 1099511628211ull;
            }
            return h;
        }
        const std::string code = g.GenerateHLSL(false) + "\x01" + g.GenerateHLSL(true);
        size_t h = std::hash<std::string>{}(code);
        h ^= (size_t)m_Document.TargetMaterial * 0x9e3779b97f4a7c15ull;       // re-apply on slot change
        h ^= (size_t)m_Document.BlendMode * 0xc2b2ae3d27d4eb4full;           // ...and on blend-mode change
        h ^= (size_t)m_Document.ShadingModel * 0x165667b19e3779f9ull;       // ...and on shading-model change
        return h;
    }

    void MaterialGraphEditor::CommitUndoIfSettled(bool interacting)
    {
        if (interacting)
            return;
        SMaterialGraphDocument cur = m_Document;
        const size_t h = cur.Hash();
        if (h == m_CommittedHash)
            return;
        m_Undo.push_back(std::move(m_Committed));
        if (m_Undo.size() > 128) m_Undo.erase(m_Undo.begin());
        m_Committed = std::move(cur);
        m_CommittedHash = h;
        m_Redo.clear();
    }


    void MaterialGraphEditor::SetDocument(const SMaterialGraphDocument& document)
    {
        Restore(document);

        // A freshly loaded document is its own baseline: it starts with no undo
        // history, and must not read as a pending edit or it would recompile at once.
        m_Undo.clear();
        m_Redo.clear();
        m_Committed = m_Document;
        m_CommittedHash = m_Document.Hash();
        m_LastStructHash = m_Document.StructHash();
        m_LastSig = Signature();
        m_DirtySince = -1.0;
    }

    void MaterialGraphEditor::Restore(const SMaterialGraphDocument& document)
    {
        m_Document = document;
        m_LinkFrom = -1;
    }

    MaterialGraph MaterialGraphEditor::Build() const
    {
        return m_Document.Build();
    }

    Ref<Material> MaterialGraphEditor::BuildMaterial(const Ref<Material>& base) const
    {
        return m_Document.BuildMaterial(AssetName(), base);
    }

    void MaterialGraphEditor::ApplyParametersTo(MaterialInstance& instance) const
    {
        m_Document.ApplyParametersTo(instance);
    }
}
