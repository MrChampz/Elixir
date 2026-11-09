#include "AnimationGraphPanel.h"
#include "../EditorStyles.h"

#include <Engine/Animation/SequenceNode.h>
#include <Engine/Animation/AnimationEvaluation.h>
#include <Engine/Font/FontManager.h>

#include <charconv>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <format>
#include <Engine/Icon/IconManager.h>

#ifdef EE_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace
{
    constexpr glm::vec2 NodeSize{210, 150};

    /** @brief Create visible text and interaction states for graph action buttons. */
    GUI::SButtonStyle GraphButtonStyle()
    {
        const auto& chrome = EditorStyle::Get();
        auto style = chrome.PanelHeaderButton;
        style.Normal.Foreground = chrome.TextPrimary;
        style.Hovered = style.Normal;
        style.Hovered->Background = chrome.PanelSection.Normal.Background;
        style.Pressed = style.Hovered;
        style.Focused = style.Hovered;
        style.Disabled = style.Normal;
        style.Disabled->Foreground.A = 0.4f;
        return style;
    }

    /**
     * @brief Parse a complete finite floating-point value.
     * @param text User-entered value.
     * @param value Receives the parsed value on success.
     */
    bool ParseFloat(const std::string& text, float& value)
    {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
               std::isfinite(value);
    }

    /**
     * @brief Add a compact action to a vertical property list.
     * @param list Container for the action.
     * @param title Action label.
     * @param callback Action to invoke on click.
     */
    void Action(
        const Ref<GUI::VerticalBox>& list,
        const std::string& title,
        const std::function<void()>& callback
    )
    {
        const auto button = CreateRef<GUI::Button>(title);
        button->SetStyle(GraphButtonStyle());
        button->SetFontSize(11);
        button->OnClick(callback);
        list->AddChild(button).SetFixedSize(28).SetHorizontalAlignment(
            GUI::EHorizontalAlignment::Fill
        );
    }

    /**
     * @brief Add an inspector caption.
     * @param list Container for the caption.
     * @param text Caption text.
     */
    void Caption(const Ref<GUI::VerticalBox>& list, const std::string& text)
    {
        const auto label = CreateRef<GUI::TextBlock>(text);
        label->SetFontSize(10);
        label->SetColor(EditorStyle::Get().TextSecondary);
        list->AddChild(label).SetFixedSize(22).SetHorizontalAlignment(
            GUI::EHorizontalAlignment::Fill
        );
    }

    /**
     * @brief Add an editable property.
     * @param list Property container.
     * @param value Initial field value.
     * @param callback Receives edits.
     */
    void Field(
        const Ref<GUI::VerticalBox>& list,
        const std::string& value,
        const std::function<void(const std::string&)>& callback
    )
    {
        const auto field = CreateRef<GUI::TextField>(value);
        field->SetStyle(EditorStyle::Get().TextField);
        field->SetFontSize(12);
        field->OnChange(callback);
        list->AddChild(field).SetFixedSize(30).SetHorizontalAlignment(
            GUI::EHorizontalAlignment::Fill
        );
    }
    /** Uses the same compact button sizing as the VFX workbench toolbar. */
    class AnimationToolbarButton final : public GUI::Button
    {
      protected:
        /**
         * @brief Measures toolbar content with the VFX minimum height.
         * @param availableSize Space offered by the toolbar.
         */
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        {
            const auto padding = GetPadding();
            const glm::vec2 inset{padding.GetTotalHorizontal(), padding.GetTotalVertical()};
            const auto content = GetContentSlot()->GetWidget();
            const auto desired =
                content ? content->Measure(glm::max(availableSize - inset, glm::vec2(0)))
                        : glm::vec2(0);
            return glm::max(desired + inset, glm::vec2(0, 24));
        }
    };

    /**
     * @brief Creates an editor label with shared typography.
     * @param text Visible label.
     * @param size Font size in GUI units.
     */
    Ref<GUI::TextBlock> WorkspaceText(const std::string& text, float size = 11)
    {
        const auto label = CreateRef<GUI::TextBlock>(text);
        label->SetFontSize(size);
        label->SetColor(EditorStyle::Get().TextPrimary);
        return label;
    }

    /**
     * @brief Loads an icon using the editor's asset root and tint.
     * @param name Icon filename without its extension.
     */
    Ref<GUI::Icon> WorkspaceIcon(const std::string& name)
    {
        const auto icon = CreateRef<GUI::Icon>();
        const auto path = std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT) / "Assets" /
                          "Icons" / (name + ".svg");
        icon->SetIcon(IconManager::Load(path.string()));
        icon->SetStyle(EditorStyle::Get().SecondaryIcon);
        icon->SetSize({12, 12});
        return icon;
    }

    /**
     * @brief Creates a compact command with the same chrome as the VFX editor.
     * @param title Visible command label, or empty for an icon-only control.
     * @param icon Icon filename without its extension.
     * @param callback Action to invoke.
     */
    Ref<GUI::Button> WorkspaceCommand(
        const std::string& title,
        const std::string& icon,
        std::function<void()> callback
    )
    {
        const auto button = CreateRef<GUI::Button>();
        button->SetStyle(GraphButtonStyle());
        button->SetPadding({6, 4});
        const auto content = CreateRef<GUI::HorizontalBox>();
        if (!icon.empty())
        {
            content->AddChild(WorkspaceIcon(icon))
                .SetFixedSize(12)
                .SetVerticalAlignment(GUI::EVerticalAlignment::Center)
                .SetMargin({0, 0, title.empty() ? 0.0f : 6.0f, 0});
        }
        if (!title.empty())
            content->AddChild(WorkspaceText(title))
                .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
        button->SetContent(content);
        button->OnClick(std::move(callback));
        return button;
    }

    /**
     * @brief Creates a rounded workspace section with an icon and divider.
     * @param title Section name.
     * @param icon Icon filename without its extension.
     */
    Ref<GUI::VerticalBox> WorkspaceSection(const std::string& title, const std::string& icon)
    {
        const auto section = CreateRef<GUI::VerticalBox>();
        section->SetStyle(EditorStyle::Get().Panel);
        section->SetPadding({1, 1});
        const auto header = CreateRef<GUI::HorizontalBox>();
        header->SetPadding({10, 0});
        header->AddChild(WorkspaceIcon(icon))
            .SetFixedSize(12)
            .SetVerticalAlignment(GUI::EVerticalAlignment::Center)
            .SetMargin({0, 0, 7, 0});
        header->AddChild(WorkspaceText(title, 12))
            .SetFillSize()
            .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
        section->AddChild(header).SetFixedSize(29).SetHorizontalAlignment(
            GUI::EHorizontalAlignment::Fill
        );
        const auto divider = CreateRef<GUI::Canvas>();
        divider->SetStyle(EditorStyle::Get().PanelHeaderBorder);
        section->AddChild(divider).SetFixedSize(1).SetHorizontalAlignment(
            GUI::EHorizontalAlignment::Fill
        );
        return section;
    }

    /**
     * @brief Adds a scrollable property column to a workspace section.
     * @param section Section receiving the scroll area.
     */
    Ref<GUI::VerticalBox> WorkspaceList(const Ref<GUI::VerticalBox>& section)
    {
        const auto scroll = CreateRef<GUI::ScrollBox>();
        scroll->SetStyle(EditorStyle::Get().ScrollBar);
        section->AddChild(scroll).SetFillSize().SetHorizontalAlignment(
            GUI::EHorizontalAlignment::Fill
        );
        const auto list = CreateRef<GUI::VerticalBox>();
        list->SetPadding({10, 6});
        scroll->SetContent(list);
        return list;
    }

    /**
     * @brief Highlights the current workspace tab or navigation row.
     * @param button Control to style.
     * @param selected True when this control represents the current selection.
     */
    void WorkspaceSelection(const Ref<GUI::Button>& button, bool selected)
    {
        auto style = GraphButtonStyle();
        if (selected)
            style.Normal.Background = EditorStyle::Get().Selection.Normal.Background;
        button->SetStyle(style);
    }
}

/** Displays the evaluated skeleton pose using the editor's axis colors. */
class AnimationPosePreview final : public GUI::Widget
{
  public:
    /**
     * @brief Displays the current runtime pose, or the origin when uncompiled.
     * @param preview Animation whose published pose is read during drawing.
     */
    void SetPreview(Ref<const Animation> preview)
    {
        m_Preview = std::move(preview);
        MarkRenderDirty();
    }

    /**
     * @brief Loads the animation skeleton and frames its reference pose.
     * @param skeleton Hierarchy shared by the animation sources, or null to clear it.
     */
    void SetSkeleton(Ref<const Skeleton> skeleton)
    {
        if (m_Skeleton == skeleton)
            return;
        m_Skeleton = std::move(skeleton);
        m_ReferencePose.Matrices.clear();
        m_Center = {};
        m_Extent = glm::vec2(1);
        m_GridScale = 1;
        if (m_Skeleton)
        {
            SLocalPose local;
            for (const auto& bone : m_Skeleton->GetBones())
                local.LocalTransforms.push_back(bone.ReferenceTransform);
            if (m_Skeleton->BuildModelPose(local, m_ReferencePose))
            {
                glm::vec2 lower = Project(glm::vec3(m_ReferencePose.Matrices.front()[3]));
                glm::vec2 upper = lower;
                for (const auto& matrix : m_ReferencePose.Matrices)
                {
                    const auto point = Project(glm::vec3(matrix[3]));
                    lower = glm::min(lower, point);
                    upper = glm::max(upper, point);
                }
                m_Center = (lower + upper) * 0.5f;
                m_Extent = glm::max(upper - lower, glm::vec2(0.5f));
                m_GridScale = std::max(m_Extent.x, m_Extent.y) * 0.25f;
            }
        }
        MarkRenderDirty();
    }

  protected:
    /**
     * @brief Uses the available preview area.
     * @param availableSize Size offered by the workspace.
     */
    glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
    {
        return glm::min(availableSize, glm::vec2(280, 180));
    }

    /**
     * @brief Draws a model-space grid, bone links, and the evaluated root.
     * @param batch Destination draw commands.
     * @param zOrder First drawing layer.
     */
    void BuildDrawCommands(GUI::RenderBatch& batch, int zOrder) override
    {
        const auto& style = EditorStyle::Get();
        batch.AddRect(
            m_Geometry,
            style.InspectorField.Normal.Background.Color,
            {},
            {},
            {},
            {},
            zOrder
        );
        if (!m_Skeleton || m_ReferencePose.Matrices.empty())
        {
            batch.AddText(
                "No animation skeleton",
                {m_Geometry.Position + glm::vec2(12), m_Geometry.Size - glm::vec2(24)},
                FontManager::GetDefaultFont(),
                11,
                style.TextSecondary,
                zOrder + 1
            );
            batch.Clip(m_Geometry);
            return;
        }
        const float scale = std::min(
            m_Geometry.Size.x * 0.8f / m_Extent.x,
            m_Geometry.Size.y * 0.8f / m_Extent.y
        );
        const bool evaluated = m_Preview && m_Preview->GetSkeleton() == m_Skeleton;
        const auto& matrices =
            evaluated ? m_Preview->GetModelPose().Matrices : m_ReferencePose.Matrices;
        const auto rootOffset = Project(glm::vec3(matrices.front()[3])) -
                                Project(glm::vec3(m_ReferencePose.Matrices.front()[3]));
        const auto center = m_Geometry.Position + m_Geometry.Size * 0.5f;
        const auto project = [this, center, scale, rootOffset](const glm::vec3& point)
        {
            return center + (Project(point) - m_Center - rootOffset) * scale;
        };
        const float gridExtent = m_GridScale * 4;
        for (int index = -4; index <= 4; ++index)
        {
            const float offset = index * m_GridScale;
            Line(
                batch,
                project({-gridExtent, 0, offset}),
                project({gridExtent, 0, offset}),
                style.PanelHeaderBorder.Normal.Background.Color,
                zOrder + 1
            );
            Line(
                batch,
                project({offset, 0, -gridExtent}),
                project({offset, 0, gridExtent}),
                style.PanelHeaderBorder.Normal.Background.Color,
                zOrder + 1
            );
        }
        const auto origin = project({0, 0, 0});
        const float axisLength = m_GridScale;
        Line(batch, origin, project({axisLength, 0, 0}), style.AxisX, zOrder + 2);
        Line(batch, origin, project({0, axisLength, 0}), style.AxisY, zOrder + 2);
        Line(batch, origin, project({0, 0, axisLength}), style.AxisZ, zOrder + 2);
        const auto& bones = m_Skeleton->GetBones();
        for (size_t index = 0; index < matrices.size(); ++index)
        {
            const auto point = project(glm::vec3(matrices[index][3]));
            const auto parent = bones[index].ParentIndex;
            if (parent >= 0)
                Line(
                    batch,
                    project(glm::vec3(matrices[parent][3])),
                    point,
                    style.TextPrimary,
                    zOrder + 3
                );
            batch.AddRect(
                {point - glm::vec2(2), {4, 4}},
                style.Accent,
                glm::vec4(2),
                {},
                {},
                {},
                zOrder + 4
            );
        }
        batch.Clip(m_Geometry);
    }

  private:
    /**
     * @brief Projects a skeleton-space position onto the preview plane.
     * @param point Position in skeleton units.
     */
    static glm::vec2 Project(const glm::vec3& point)
    {
        return {point.x - point.z * 0.55f, -point.y + point.z * 0.3f};
    }

    /**
     * @brief Draws a projected bone or grid segment.
     * @param batch Destination draw commands.
     * @param from First endpoint.
     * @param to Last endpoint.
     * @param color Segment color.
     * @param zOrder Drawing layer.
     */
    void Line(
        GUI::RenderBatch& batch,
        glm::vec2 from,
        glm::vec2 to,
        const GUI::SColor& color,
        int zOrder
    ) const
    {
        const int steps = std::max(1, int(glm::length(to - from)));
        for (int index = 0; index <= steps; ++index)
        {
            const auto point = glm::mix(from, to, float(index) / steps);
            batch.AddRect({point, {1.5f, 1.5f}}, color, {}, {}, {}, {}, zOrder);
        }
    }

    Ref<const Animation> m_Preview;
    Ref<const Skeleton> m_Skeleton;
    SModelPose m_ReferencePose;
    /** Stable reference-pose bounds prevent the camera from moving during playback. */
    glm::vec2 m_Center{}, m_Extent{1};
    float m_GridScale = 1;
};

/** Draws asset references and pose links over the complete document workspace. */
class AnimationGraphCanvas final : public GUI::Widget
{
  public:
    /**
     * @brief Retains the graph document owned by the panel.
     * @param document Document that outlives this canvas.
     */
    explicit AnimationGraphCanvas(AnimationGraphDocument& document) : m_Document(document) {}

    /** Receives asset node selections; -1 selects the graph output. */
    std::function<void(EAnimationAssetKind, int)> SelectAsset;
    /** Receives selection of the state machine reference. */
    std::function<void()> SelectMachine;
    /** Receives a pose source dropped onto the graph output; empty selects the machine. */
    std::function<void(const std::string&)> ConnectOutput;
    /** Receives completed moves of authored asset nodes. */
    std::function<void()> Edited;

    /**
     * @brief Updates the selected reference without entering its asset editor.
     * @param kind Selected asset category.
     * @param index Asset index, or -1 for the output.
     * @param machine True selects the state machine reference.
     */
    void SetSelection(EAnimationAssetKind kind, int index, bool machine)
    {
        if (m_AssetKind == kind && m_AssetIndex == index && m_MachineSelected == machine)
            return;
        m_AssetKind = kind;
        m_AssetIndex = index;
        m_MachineSelected = machine;
        MarkRenderDirty();
    }

    /** @brief Fits the graph between the floating tools on its next draw. */
    void ResetView()
    {
        m_FrameRequested = true;
        MarkRenderDirty();
    }

    /** @brief Reflows the workspace after floating tool content changes. */
    void ReflowWorkspace() { MarkLayoutDirty(); }

    /** @brief Invalidates cached nodes and links after a document edit. */
    void DocumentChanged() { MarkRenderDirty(); }

    /** @brief Claims pointer events for node selection, links, and canvas navigation. */
    bool CanHandleMouseInput() const override { return IsEnabled(); }

  protected:
    /**
     * @brief Uses the complete document viewport.
     * @param availableSize Size offered by the document tab.
     */
    glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
    {
        return availableSize;
    }

    /**
     * @brief Draws the workbench grid, curved pose links, and asset columns.
     * @param batch Destination draw commands.
     * @param zOrder First draw layer.
     */
    void BuildDrawCommands(GUI::RenderBatch& batch, int zOrder) override
    {
        const auto& style = EditorStyle::Get();
        const auto nodes = Nodes();
        if (m_FrameRequested && m_Geometry.Size.x > 0 && m_Geometry.Size.y > 0)
            Frame(nodes);
        batch.AddRect(m_Geometry, {0.05f, 0.055f, 0.065f, 1}, {}, {}, {}, {}, zOrder);
        const float step = 32 * m_Zoom;
        for (float x = std::fmod(m_Pan.x, step); x < m_Geometry.Size.x; x += step)
            for (float y = std::fmod(m_Pan.y, step); y < m_Geometry.Size.y; y += step)
                batch.AddRect(
                    {m_Geometry.Position + glm::vec2(x, y), {1, 1}},
                    style.PanelHeaderBorder.Normal.Background.Color,
                    {},
                    {},
                    {},
                    {},
                    zOrder
                );
        const auto& output = nodes.back();
        for (const auto& node : nodes)
        {
            if (HasPose(node) && node.Source == m_Document.OutputSource)
                Curve(batch, Pin(node, true), Pin(output, false), zOrder + 1);
        }
        if (m_Link >= 0 && size_t(m_Link) < nodes.size())
            Curve(batch, Pin(nodes[m_Link], true), m_LastMouse, zOrder + 1);
        for (const auto& node : nodes)
            DrawNode(batch, node, zOrder + 2);
        batch.Clip(m_Geometry);
    }

    /**
     * @brief Selects a reference, begins a pose link, or captures a canvas pan.
     * @param event Pointer press in GUI coordinates.
     */
    GUI::SInputReply HandleMouseDown(const MouseButtonPressedEvent& event) override
    {
        m_LastMouse = {event.GetX(), event.GetY()};
        m_Pressed = true;
        m_Moved = false;
        m_Drag = -1;
        m_Link = -1;
        if (event.GetMouseButton() != 0)
            return GUI::SInputReply::HandledAndCaptured();
        const auto nodes = Nodes();
        for (size_t index = nodes.size(); index > 0; --index)
        {
            const auto& node = nodes[index - 1];
            if (HasPose(node) && glm::distance(Pin(node, true), m_LastMouse) < 10)
            {
                m_Link = int(index - 1);
                return GUI::SInputReply::HandledAndCaptured();
            }
            if (!NodeRect(node).Contains(m_LastMouse))
                continue;
            m_Drag = int(index - 1);
            if (node.Role == ERole::Machine && SelectMachine)
                SelectMachine();
            else if (SelectAsset)
                SelectAsset(node.Kind, node.Index);
            return GUI::SInputReply::HandledAndCaptured();
        }
        return GUI::SInputReply::HandledAndCaptured();
    }

    /**
     * @brief Moves a node, updates the pending curve, or pans the workspace.
     * @param event Current pointer position.
     */
    GUI::SInputReply HandleMouseMove(const MouseMovedEvent& event) override
    {
        const glm::vec2 point{event.GetX(), event.GetY()};
        if (m_Pressed)
        {
            const auto delta = point - m_LastMouse;
            const auto nodes = Nodes();
            if (m_Drag >= 0 && size_t(m_Drag) < nodes.size())
                *nodes[m_Drag].Position += delta / m_Zoom;
            else if (m_Link < 0)
                m_Pan += delta;
            m_Moved = m_Moved || glm::length(delta) > 0;
            MarkRenderDirty();
        }
        m_LastMouse = point;
        return GUI::SInputReply::Handled();
    }

    /**
     * @brief Commits an authored move or connects a pose to the output pin.
     * @param event Released pointer button and position.
     */
    GUI::SInputReply HandleMouseUp(const MouseButtonReleasedEvent& event) override
    {
        const auto nodes = Nodes();
        const glm::vec2 point{event.GetX(), event.GetY()};
        if (m_Link >= 0 && size_t(m_Link) < nodes.size() && ConnectOutput &&
            glm::distance(Pin(nodes.back(), false), point) < 14)
            ConnectOutput(nodes[m_Link].Source);
        if (m_Drag >= 0 && size_t(m_Drag) < nodes.size() && m_Moved &&
            nodes[m_Drag].Role == ERole::Asset && Edited)
            Edited();
        m_Pressed = false;
        m_Link = -1;
        m_Drag = -1;
        MarkRenderDirty();
        return GUI::SInputReply::Handled();
    }

    /**
     * @brief Zooms around the current pointer position.
     * @param event Scroll-wheel delta.
     */
    GUI::SInputReply HandleMouseScrolled(const MouseScrolledEvent& event) override
    {
        const float old = m_Zoom;
        m_Zoom = std::clamp(m_Zoom * std::pow(1.1f, event.GetOffsetY()), 0.4f, 1.8f);
        const auto local = m_LastMouse - m_Geometry.Position;
        m_Pan = local - (local - m_Pan) * (m_Zoom / old);
        MarkRenderDirty();
        return GUI::SInputReply::Handled();
    }

  private:
    /** Identifies graph references without entering their internal asset graphs. */
    enum class ERole
    {
        Asset,
        Machine,
        Output
    };

    /** Describes one reference column; the document retains authored asset positions. */
    struct SNode
    {
        ERole Role;
        EAnimationAssetKind Kind;
        int Index;
        std::string Name;
        std::string Type;
        std::string Source;
        glm::vec2* Position;
    };

    /** @brief Lists asset references followed by the machine and output nodes. */
    std::vector<SNode> Nodes()
    {
        std::vector<SNode> nodes;
        auto& library = m_Document.Library;
        for (size_t index = 0; index < library.Sequences.size(); ++index)
        {
            auto& asset = library.Sequences[index];
            nodes.push_back(
                {ERole::Asset,
                 EAnimationAssetKind::Sequence,
                 int(index),
                 asset.Name,
                 "Sequence",
                 asset.Name,
                 &asset.Position}
            );
        }
        for (size_t index = 0; index < library.BlendSpaces.size(); ++index)
        {
            auto& asset = library.BlendSpaces[index];
            nodes.push_back(
                {ERole::Asset,
                 EAnimationAssetKind::BlendSpace,
                 int(index),
                 asset.Name,
                 asset.TwoDimensional ? "Blend Space 2D" : "Blend Space 1D",
                 asset.Name,
                 &asset.Position}
            );
        }
        nodes.push_back(
            {ERole::Machine,
             EAnimationAssetKind::Sequence,
             -1,
             "State Machine",
             "State Machine",
             "",
             &m_MachinePosition}
        );
        nodes.push_back(
            {ERole::Output,
             EAnimationAssetKind::Sequence,
             -1,
             "Output Pose",
             "Animation Output",
             "",
             &m_OutputPosition}
        );
        return nodes;
    }

    /**
     * @brief Fits visible nodes between the preview and property overlays.
     * @param nodes References whose bounds determine the workspace camera.
     */
    void Frame(const std::vector<SNode>& nodes)
    {
        glm::vec2 lower = *nodes.front().Position;
        glm::vec2 upper = lower + NodeSize;
        for (const auto& node : nodes)
        {
            lower = glm::min(lower, *node.Position);
            upper = glm::max(upper, *node.Position + NodeSize);
        }
        const auto extent = glm::max(upper - lower, glm::vec2(1));
        const glm::vec2 available{
            std::max(200.0f, m_Geometry.Size.x - 510),
            std::max(180.0f, m_Geometry.Size.y - 110)
        };
        m_Zoom =
            std::clamp(std::min(available.x / extent.x, available.y / extent.y), 0.4f, 1.0f);
        m_Pan = glm::vec2(220, 70) + (available - extent * m_Zoom) * 0.5f - lower * m_Zoom;
        m_FrameRequested = false;
    }

    /**
     * @brief Reports whether a node can supply the graph's base pose.
     * @param node Reference to inspect.
     */
    bool HasPose(const SNode& node) const { return node.Role != ERole::Output; }

    /**
     * @brief Converts a reference position to its screen bounds.
     * @param node Reference to place.
     */
    GUI::SRect NodeRect(const SNode& node) const
    {
        return {m_Geometry.Position + m_Pan + *node.Position * m_Zoom, NodeSize * m_Zoom};
    }

    /**
     * @brief Gets a pose pin on either side of a node.
     * @param node Reference containing the pin.
     * @param output True for the right output; false for the left input.
     */
    glm::vec2 Pin(const SNode& node, bool output) const
    {
        const auto rect = NodeRect(node);
        return rect.Position + glm::vec2(output ? rect.Size.x : 0, 130 * m_Zoom);
    }

    /**
     * @brief Draws a smooth cubic pose connection with horizontal endpoint tangents.
     * @param batch Destination draw commands.
     * @param from Output pin position.
     * @param to Input pin position.
     * @param layer Draw layer below nodes.
     */
    void Curve(GUI::RenderBatch& batch, glm::vec2 from, glm::vec2 to, int layer) const
    {
        const float reach = std::max(60 * m_Zoom, std::abs(to.x - from.x) * 0.5f);
        const auto first = from + glm::vec2(reach, 0);
        const auto second = to - glm::vec2(reach, 0);
        const float length = glm::distance(from, first) + glm::distance(first, second) +
                             glm::distance(second, to);
        const int steps = int(std::clamp(length / 0.35f, 1.0f, 4096.0f));
        for (int index = 0; index <= steps; ++index)
        {
            const float t = float(index) / float(steps);
            const float u = 1 - t;
            const auto point = u * u * u * from + 3 * u * u * t * first +
                               3 * u * t * t * second + t * t * t * to;
            batch.AddRect(
                {point - glm::vec2(1), {2, 2}},
                EditorStyle::Get().Accent,
                glm::vec4(1),
                {},
                {},
                {},
                layer
            );
        }
    }

    /**
     * @brief Draws a reference with the header, framed body, and rows used by VFX columns.
     * @param batch Destination draw commands.
     * @param node Reference to draw.
     * @param layer First node draw layer.
     */
    void DrawNode(GUI::RenderBatch& batch, const SNode& node, int layer) const
    {
        const auto& style = EditorStyle::Get();
        const auto rect = NodeRect(node);
        const bool selected =
            node.Role == ERole::Machine ? m_MachineSelected
            : node.Role == ERole::Output
                ? !m_MachineSelected && m_AssetIndex < 0
                : !m_MachineSelected && node.Kind == m_AssetKind && node.Index == m_AssetIndex;
        auto card = style.PanelSection.Normal.Background;
        card.CornerRadius = glm::vec4(6 * m_Zoom);
        card.Outline = {
            selected ? style.Accent : style.PanelHeaderBorder.Normal.Background.Color,
            1
        };
        batch.AddBrush(card, rect, layer);
        const auto color = node.Role == ERole::Machine ? style.Accent
                           : node.Role == ERole::Output
                               ? style.PanelHeaderBorder.Normal.Background.Color
                           : node.Kind == EAnimationAssetKind::Sequence ? style.AxisZ
                                                                        : style.AxisY;
        batch.AddRect(
            {rect.Position, {rect.Size.x, 26 * m_Zoom}},
            color,
            {6 * m_Zoom, 6 * m_Zoom, 0, 0},
            {},
            {},
            {},
            layer + 1
        );
        Label(
            batch,
            node.Name,
            rect.Position + glm::vec2(8, 5) * m_Zoom,
            style.TextPrimary,
            11,
            layer + 2
        );
        Label(
            batch,
            node.Type,
            rect.Position + glm::vec2(8, 34) * m_Zoom,
            style.TextSecondary,
            10,
            layer + 2
        );
        const GUI::SRect body{
            rect.Position + glm::vec2(6, 56) * m_Zoom,
            glm::vec2(NodeSize.x - 12, 48) * m_Zoom
        };
        auto section = style.InspectorField.Normal.Background;
        section.CornerRadius = glm::vec4(3 * m_Zoom);
        batch.AddBrush(section, body, layer + 1);
        Label(
            batch,
            node.Role == ERole::Output ? "Result" : "Asset",
            body.Position + glm::vec2(6, 4) * m_Zoom,
            style.TextSecondary,
            10,
            layer + 2
        );
        const auto reference = node.Role == ERole::Machine ? m_Document.Name
                               : node.Role == ERole::Output
                                   ? std::string("Final animation pose")
                                   : node.Name;
        Label(
            batch,
            reference,
            body.Position + glm::vec2(6, 22) * m_Zoom,
            style.TextPrimary,
            11,
            layer + 2
        );
        if (HasPose(node) || node.Role == ERole::Output)
        {
            const auto point = Pin(node, node.Role != ERole::Output);
            const float radius = 4 * m_Zoom;
            batch.AddRect(
                {point - glm::vec2(radius), glm::vec2(radius * 2)},
                style.Accent,
                glm::vec4(radius),
                {},
                {},
                {},
                layer + 3
            );
            Label(
                batch,
                "Pose",
                rect.Position +
                    glm::vec2(node.Role == ERole::Output ? 10 : NodeSize.x - 40, 122) * m_Zoom,
                style.TextPrimary,
                11,
                layer + 2
            );
        }
    }

    /**
     * @brief Draws a scaled label inside a reference column.
     * @param batch Destination draw commands.
     * @param text Visible label.
     * @param position Screen position.
     * @param color Text color.
     * @param size Font size before zoom.
     * @param layer Draw layer.
     */
    void Label(
        GUI::RenderBatch& batch,
        const std::string& text,
        glm::vec2 position,
        const GUI::SColor& color,
        float size,
        int layer
    ) const
    {
        batch.AddText(
            text,
            {position, {190 * m_Zoom, 20 * m_Zoom}},
            FontManager::GetDefaultFont(),
            size * m_Zoom,
            color,
            layer
        );
    }

    AnimationGraphDocument& m_Document;
    /** Workspace locations of the machine reference and final output. */
    glm::vec2 m_MachinePosition{520, 300}, m_OutputPosition{800, 100};
    glm::vec2 m_Pan{}, m_LastMouse{};
    float m_Zoom = 1;
    int m_AssetIndex = -1, m_Drag = -1, m_Link = -1;
    EAnimationAssetKind m_AssetKind = EAnimationAssetKind::Sequence;
    bool m_MachineSelected = false, m_Pressed = false, m_Moved = false;
    bool m_FrameRequested = true;
};

AnimationGraphPanel::AnimationGraphPanel()
{
    const auto skeleton = Skeleton::Create("Graph Preview", {{"Root"}});
    float height = 0;
    for (const auto& name : {"Idle", "Walk", "Run"})
    {
        SAnimationSequenceAsset sequence;
        sequence.Name = name;
        sequence.Skeleton = skeleton;
        sequence.Position = {0, height * 360};
        SBoneAnimationTrack track;
        track.Translations = {{0, {0, height, 0}}, {1, {0, height + 0.1f, 0}}};
        sequence.Tracks.push_back(track);
        sequence.Markers = {{"Start", 0}, {"Contact", 0.5f}};
        sequence.Notifies = {{.Name = "Footstep", .Time = 0.5f}};
        m_Document.Library.Sequences.push_back(sequence);
        height += 0.5f;
    }
    m_Document.Library.BlendSpaces = {
        {"Locomotion1D",
         false,
         "Speed",
         "Direction",
         {{"Idle", {0, 0}}, {"Walk", {1.5f, 0}}, {"Run", {4, 0}}},
         {260, 30}},
        {"Directional2D",
         true,
         "Speed",
         "Direction",
         {{"Idle", {0, 0}}, {"Walk", {1, 0}}, {"Run", {0, 1}}},
         {260, 240}}
    };
    m_Document.Library.Montages = {
        {"Action",
         "Run",
         0.15f,
         0.15f,
         true,
         {{"Start", 0}, {"Action", 0.4f, std::string("Action")}, {"End", 0.8f}},
         {520, 480}}
    };
    RefreshSources();
    m_Document.Name = "Locomotion";
    m_Document.Parameters = {{"Speed", 0.0f}, {"Direction", 0.0f}};
    m_Document.Machine.InitialState = "Idle";
    m_Document.Machine.States = {
        {"Idle", m_Sources.at("Idle")},
        {"Walk", m_Sources.at("Walk")},
        {"Run", m_Sources.at("Run")}
    };
    m_Document.Positions = {{0, 100}, {260, 100}, {520, 100}};
    m_Document.StateSources = {"Idle", "Walk", "Run"};
    m_Document.OutputSource = "Locomotion1D";
    m_Document.Machine.Transitions = {
        {"Idle", "Walk", {{"Speed", EAnimationComparison::Greater, 0.1f}}},
        {"Walk", "Idle", {{"Speed", EAnimationComparison::LessEqual, 0.1f}}},
        {"Walk", "Run", {{"Speed", EAnimationComparison::Greater, 3.0f}}},
        {"Run", "Walk", {{"Speed", EAnimationComparison::LessEqual, 3.0f}}}
    };
    m_LastEdit = m_Document;
    m_AssetKind = EAnimationAssetKind::BlendSpace;
    m_Asset = 0;
}

AnimationGraphPanel::AnimationGraphPanel(
    AnimationGraphDocument document,
    AnimationGraphDocument::Sources sources
)
    : m_Document(std::move(document)), m_Sources(std::move(sources)), m_LastEdit(m_Document)
{
    m_Document.Positions.resize(m_Document.Machine.States.size(), {80, 100});
    if (m_Document.StateSources.empty())
    {
        for (const auto& state : m_Document.Machine.States)
        {
            std::string name;
            for (const auto& [key, node] : m_Sources)
                if (node == state.Node)
                    name = key;
            m_Document.StateSources.push_back(name);
        }
    }
    RefreshSources();
    m_LastEdit = m_Document;
}

Ref<GUI::Widget> AnimationGraphPanel::Build()
{
    const auto root = CreateRef<GUI::Canvas>();
    const auto& chrome = EditorStyle::Get();
    root->SetStyle(chrome.Transparent);
    m_Canvas = CreateRef<AnimationGraphCanvas>(m_Document);
    root->AddChild(m_Canvas).SetAnchors(GUI::SAnchors::StretchAll());
    m_Canvas->SelectAsset = [this](EAnimationAssetKind kind, int index)
    {
        m_AssetKind = kind;
        m_Asset = index;
        m_MachineSelected = false;
        m_ShowDefaults = false;
        m_Refresh = true;
    };
    m_Canvas->SelectMachine = [this]
    {
        m_MachineSelected = true;
        m_Asset = -1;
        m_ShowDefaults = false;
        m_Refresh = true;
    };
    m_Canvas->ConnectOutput = [this](const std::string& source)
    {
        if (m_Document.OutputSource == source)
            return;
        m_Document.OutputSource = source;
        Changed();
    };
    m_Canvas->Edited = [this]
    {
        Changed();
    };

    const auto tools = CreateRef<GUI::Overlay>();
    tools->SetStyle(chrome.Toolbar);
    tools->SetPadding({4, 4});
    root->AddChild(tools)
        .SetAnchors(GUI::SAnchors::TopCenter())
        .SetAlignment({0.5f, 0})
        .SetPosition({0, 10})
        .SetSize({160, 34});
    const auto toolbar = CreateRef<GUI::HorizontalBox>();
    tools->AddChild(toolbar)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    const auto command =
        [&](const std::string& name, const std::string& label, std::function<void()> callback)
    {
        const auto button = CreateRef<AnimationToolbarButton>();
        button->SetStyle(chrome.PanelHeaderButton);
        button->SetPadding({6, 4});
        button->OnClick(std::move(callback));
        const auto content = CreateRef<GUI::HorizontalBox>();
        const auto icon = WorkspaceIcon(name);
        icon->SetSize({12, 12});
        content->AddChild(icon).SetFixedSize(13).SetMargin({0, 0, 5, 0});
        content->AddChild(WorkspaceText(label, 11));
        button->SetContent(content);
        toolbar->AddChild(button).SetFillSize();
    };
    command(
        "save",
        "Save",
        [this]
        {
            FileAction(true);
        }
    );
    command(
        "compile",
        "Compile",
        [this]
        {
            Compile();
        }
    );
    RegisterOverlay(tools);

    const auto title = CreateRef<GUI::Overlay>();
    title->SetStyle(chrome.Panel);
    title->SetPadding({10, 0});
    root->AddChild(title).SetPosition({10, 10}).SetSize({190, 34});
    m_DocumentTitle = WorkspaceText(m_Document.Name, 12);
    title->AddChild(m_DocumentTitle)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    RegisterOverlay(title);

    const auto preview = WorkspaceSection("Preview", "eye");
    root->AddChild(preview).SetPosition({10, 56}).SetSize({190, 264});
    m_PosePreview = CreateRef<AnimationPosePreview>();
    preview->AddChild(m_PosePreview)
        .SetFillSize()
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    const auto playback = CreateRef<GUI::HorizontalBox>();
    playback->SetPadding({6, 2});
    playback
        ->AddChild(WorkspaceCommand(
            "",
            "restart",
            [this]
            {
                SeekPreview(0);
            }
        ))
        .SetFixedSize(28);
    m_PlayButton = WorkspaceCommand(
        "",
        "play",
        [this]
        {
            if (!m_Preview)
                Compile();
            m_Playing = m_Preview && !m_Playing;
        }
    );
    playback->AddChild(m_PlayButton).SetFixedSize(28);
    playback
        ->AddChild(WorkspaceCommand(
            "",
            "pause",
            [this]
            {
                m_Playing = false;
            }
        ))
        .SetFixedSize(28);
    playback
        ->AddChild(WorkspaceCommand(
            "",
            "step-forward",
            [this]
            {
                if (!m_Preview)
                    Compile();
                m_Playing = false;
                if (m_Preview)
                    m_Preview->Update(1.0f / 60);
            }
        ))
        .SetFixedSize(28);
    m_PlaybackStatus = WorkspaceText("0.00 s", 10);
    playback->AddChild(m_PlaybackStatus)
        .SetFillSize()
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center)
        .SetMargin({8, 0, 0, 0});
    preview->AddChild(playback).SetFixedSize(30).SetHorizontalAlignment(
        GUI::EHorizontalAlignment::Fill
    );
    m_PoseStatus = WorkspaceText("Compile to preview the skeleton pose", 10);
    preview->AddChild(m_PoseStatus)
        .SetFixedSize(24)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetMargin({10, 0});

    RegisterOverlay(preview, {-12, 0});
    const auto inputs = WorkspaceSection("Preview Parameters", "edit");
    root->AddChild(inputs).SetPosition({10, 330}).SetSize({190, 180});
    m_PreviewInputs = WorkspaceList(inputs);
    RegisterOverlay(inputs, {-12, 0});

    const auto details = WorkspaceSection("Node Properties", "inspector");
    root->AddChild(details)
        .SetAnchors({1, 0, 1, 1})
        .SetPosition({-270, 0})
        .SetSize({260, 0})
        .SetOffsets(0, 10, 0, -10);
    const auto tabs = CreateRef<GUI::HorizontalBox>();
    m_DetailsTab = WorkspaceCommand(
        "Node",
        "",
        [this]
        {
            m_ShowDefaults = false;
            m_Refresh = true;
        }
    );
    m_DefaultsTab = WorkspaceCommand(
        "Graph",
        "",
        [this]
        {
            m_ShowDefaults = true;
            m_Refresh = true;
        }
    );
    tabs->AddChild(m_DetailsTab).SetFillSize();
    tabs->AddChild(m_DefaultsTab).SetFillSize();
    details->AddChild(tabs).SetFixedSize(28).SetHorizontalAlignment(
        GUI::EHorizontalAlignment::Fill
    );
    m_Inspector = WorkspaceList(details);
    RegisterOverlay(details, {12, 0});
    m_Path = CreateRef<GUI::TextField>("AnimationGraph.animgraph");
    m_Path->SetStyle(chrome.TextField);
    m_Path->SetFontSize(11);

    m_Status = WorkspaceText("Ready", 10);
    m_Status->SetColor(chrome.TextSecondary);
    root->AddChild(m_Status)
        .SetAnchors({0, 1, 1, 1})
        .SetPosition({0, -28})
        .SetSize({0, 20})
        .SetOffsets(220, 0, -290, 0);
    m_Status->SetVisibility(GUI::EVisibility::HitTestInvisible);
    RegisterOverlay(m_Status, {0, 12});
    RefreshInspector();
    Compile();
    return root;
}

void AnimationGraphPanel::Changed()
{
    RefreshSources();
    m_Undo.push_back(m_LastEdit);
    if (m_Undo.size() > 100)
        m_Undo.erase(m_Undo.begin());
    m_Redo.clear();
    m_LastEdit = m_Document;
    m_Preview.reset();
    m_Playing = false;
    m_Dirty = true;
    m_Refresh = true;
    if (m_Canvas)
        m_Canvas->DocumentChanged();
    if (m_Status)
        m_Status->SetText("Modified. Compile to update the preview.");
}

void AnimationGraphPanel::Compile()
{
    const auto error = m_Document.Validate();
    if (!error.empty())
    {
        m_Status->SetText(error);
        return;
    }
    const auto graph = m_Document.Compile();
    if (!graph)
    {
        m_Status->SetText("Runtime rejected the graph.");
        return;
    }
    SLocalPose reference;
    for (const auto& bone : graph->GetSkeleton()->GetBones())
        reference.LocalTransforms.push_back(bone.ReferenceTransform);
    m_Preview = graph->CreateAnimation(reference);
    if (m_Preview) m_Preview->SetManaged(false);
    m_Status->SetText(
        m_Preview ? "Compiled. Set live parameters and press Play."
                  : "Could not create preview."
    );
    m_Refresh = true;
}

void AnimationGraphPanel::History(const bool redo)
{
    auto& from = redo ? m_Redo : m_Undo;
    auto& to = redo ? m_Undo : m_Redo;
    if (from.empty())
        return;
    to.push_back(m_Document);
    m_Document = std::move(from.back());
    from.pop_back();
    m_LastEdit = m_Document;
    m_Asset = -1;
    RefreshSources();
    m_Preview.reset();
    m_Playing = false;
    m_Dirty = true;
    m_Refresh = true;
    m_Status->SetText("History restored. Compile to update the preview.");
    m_Canvas->DocumentChanged();
}

void AnimationGraphPanel::FileAction(const bool save)
{
    if (save)
    {
        std::ostringstream serialized;
        if (!m_Document.Write(serialized, m_Sources))
        {
            const auto error = m_Document.Validate();
            m_Status->SetText(
                "Cannot save: " +
                (error.empty()
                     ? std::string("unregistered source or unsupported notify payload.")
                     : error)
            );
            return;
        }
        const std::filesystem::path destination(m_Path->GetText());
        auto temporary = destination;
        temporary += ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << serialized.str();
        output.close();
        if (!output)
        {
            m_Status->SetText("Save failed. Check the path and permissions.");
            return;
        }
        // Replace only after the complete new document has reached the temporary file.
#ifdef EE_PLATFORM_WINDOWS
        const bool replaced = MoveFileExW(
                                  temporary.c_str(),
                                  destination.c_str(),
                                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
                              ) != 0;
#else
        std::error_code error;
        std::filesystem::rename(temporary, destination, error);
        const bool replaced = !error;
#endif
        if (!replaced)
        {
            m_Status->SetText("Could not replace the saved graph. Temporary file retained.");
            return;
        }
        m_Dirty = false;
        m_Status->SetText("Saved " + m_Path->GetText());
    }
    else
    {
        if (m_Dirty)
        {
            m_Status->SetText("Save current edits before loading another graph.");
            return;
        }
        std::ifstream input(m_Path->GetText(), std::ios::binary);
        if (!m_Document.Read(input, m_Sources))
        {
            m_Status->SetText("Load failed: invalid file or unavailable source.");
            return;
        }
        Changed();
        m_Dirty = false;
        m_Asset = -1;
        m_Status->SetText("Loaded " + m_Path->GetText());
    }
}

void AnimationGraphPanel::OnUpdate(const Timestep frameTime)
{
    if (m_Refresh)
    {
        m_Refresh = false;
        RefreshInspector();
    }
    m_DocumentTitle->SetText(m_Document.Name + (m_Dirty ? " *" : ""));
    if (m_Preview && m_Playing)
    {
        if (!m_Preview->Update(std::min(float(frameTime), 0.1f)))
        {
            m_Playing = false;
            m_Status->SetText("Runtime preview failed to advance.");
        }
    }
    m_Canvas->SetSelection(m_AssetKind, m_Asset, m_MachineSelected);
    Ref<const Skeleton> skeleton = m_Preview ? m_Preview->GetSkeleton() : nullptr;
    if (m_Asset >= 0 && m_AssetKind == EAnimationAssetKind::Sequence &&
        size_t(m_Asset) < m_Document.Library.Sequences.size())
        skeleton = m_Document.Library.Sequences[m_Asset].Skeleton;
    else
    {
        std::string sourceName = m_Document.OutputSource;
        if (m_Asset >= 0 && m_AssetKind == EAnimationAssetKind::BlendSpace &&
            size_t(m_Asset) < m_Document.Library.BlendSpaces.size())
            sourceName = m_Document.Library.BlendSpaces[m_Asset].Name;
        const auto source = m_Sources.find(sourceName);
        if (source != m_Sources.end() && source->second)
            skeleton = source->second->GetSkeleton();
        else if (!skeleton && !m_Document.Machine.States.empty() &&
                 m_Document.Machine.States.front().Node)
            skeleton = m_Document.Machine.States.front().Node->GetSkeleton();
    }
    m_PosePreview->SetSkeleton(skeleton);
    m_PosePreview->SetPreview(m_Preview);
    auto playStyle = GraphButtonStyle();
    if (m_Playing)
        playStyle.Normal.Background = EditorStyle::Get().PlayButtonActive.Normal.Background;
    m_PlayButton->SetStyle(playStyle);
    if (m_Preview)
    {
        const auto& player = m_Preview->GetBaseLayer().GetPlayer();
        const double time = player.GetTime();
        m_PlaybackStatus->SetText(std::format("{:.2f} s", time));
    }
    else
    {
        m_PlaybackStatus->SetText("Not compiled");
    }
    if (skeleton)
        m_PoseStatus->SetText(
            std::format("{} · {} bones", skeleton->GetName(), skeleton->GetBones().size())
        );
    else
        m_PoseStatus->SetText("No animation skeleton");
}

void AnimationGraphPanel::RefreshInspector()
{
    // Reflow the overlay tree when scroll content is replaced after a drawer transition.
    m_Canvas->ReflowWorkspace();
    m_Inspector->ClearChildren();
    RefreshPreviewInputs();
    WorkspaceSelection(m_DetailsTab, !m_ShowDefaults);
    WorkspaceSelection(m_DefaultsTab, m_ShowDefaults);
    if (m_ShowDefaults)
    {
        BuildDefaultsInspector();
        return;
    }
    std::string name = m_MachineSelected ? m_Document.Name : "Output Pose";
    std::string type = m_MachineSelected ? "State Machine" : "Animation Output";
    const auto& library = m_Document.Library;
    bool source = m_MachineSelected;
    if (m_Asset >= 0)
    {
        const size_t index = size_t(m_Asset);
        if (m_AssetKind == EAnimationAssetKind::Sequence && index < library.Sequences.size())
        {
            name = library.Sequences[index].Name;
            type = "Sequence";
            source = true;
        }
        else if (m_AssetKind == EAnimationAssetKind::BlendSpace &&
                 index < library.BlendSpaces.size())
        {
            name = library.BlendSpaces[index].Name;
            type = library.BlendSpaces[index].TwoDimensional ? "Blend Space 2D"
                                                             : "Blend Space 1D";
            source = true;
        }
        else
            m_Asset = -1;
    }
    Caption(m_Inspector, "NODE");
    Caption(m_Inspector, name);
    Caption(m_Inspector, type);
    if (source)
    {
        const std::string sourceName = m_MachineSelected ? "" : name;
        Action(
            m_Inspector,
            "Connect to Output Pose",
            [this, sourceName]
            {
                if (m_Document.OutputSource != sourceName)
                {
                    m_Document.OutputSource = sourceName;
                    Changed();
                }
            }
        );
    }
    Caption(m_Inspector, "GRAPH OUTPUT");
    Caption(
        m_Inspector,
        m_Document.OutputSource.empty() ? "State Machine" : m_Document.OutputSource
    );
    Action(
        m_Inspector,
        m_Document.OutputLooping ? "Playback: Loop" : "Playback: Once",
        [this]
        {
            m_Document.OutputLooping = !m_Document.OutputLooping;
            Changed();
        }
    );
}

void AnimationGraphPanel::BuildDefaultsInspector()
{
    Caption(m_Inspector, "DOCUMENT");
    Field(
        m_Inspector,
        m_Document.Name,
        [this](const std::string& value)
        {
            m_Document.Name = value;
            Changed();
            m_Refresh = false;
        }
    );
    Caption(m_Inspector, "File");
    m_Inspector->AddChild(m_Path).SetFixedSize(30).SetHorizontalAlignment(
        GUI::EHorizontalAlignment::Fill
    );
    Action(
        m_Inspector,
        "Open graph",
        [this]
        {
            FileAction(false);
        }
    );
    Action(
        m_Inspector,
        "Undo",
        [this]
        {
            History(false);
        }
    );
    Action(
        m_Inspector,
        "Redo",
        [this]
        {
            History(true);
        }
    );
    Caption(m_Inspector, "PARAMETERS / DEFAULTS");
    for (size_t i = 0; i < m_Document.Parameters.size(); ++i)
    {
        const auto& parameter = m_Document.Parameters[i];
        Caption(m_Inspector, parameter.Name);
        Field(
            m_Inspector,
            parameter.Name,
            [this, i](const std::string& name)
            {
                if (name.empty())
                    return;
                for (size_t j = 0; j < m_Document.Parameters.size(); ++j)
                    if (j != i && m_Document.Parameters[j].Name == name)
                        return;
                const auto previous = m_Document.Parameters[i].Name;
                m_Document.Parameters[i].Name = name;
                for (auto& state : m_Document.Machine.States)
                    for (auto& binding : state.InputParameters)
                        if (binding == previous)
                            binding = name;
                for (auto& transition : m_Document.Machine.Transitions)
                    for (auto& condition : transition.Conditions)
                        if (condition.Parameter == previous)
                            condition.Parameter = name;
                for (auto& blend : m_Document.Library.BlendSpaces)
                {
                    if (blend.XParameter == previous)
                        blend.XParameter = name;
                    if (blend.YParameter == previous)
                        blend.YParameter = name;
                }
                Changed();
                m_Refresh = false;
            }
        );
        const bool boolean = std::holds_alternative<bool>(parameter.DefaultValue);
        const auto value = boolean
                               ? (std::get<bool>(parameter.DefaultValue) ? "true" : "false")
                               : std::to_string(std::get<float>(parameter.DefaultValue));
        Field(
            m_Inspector,
            value,
            [this, i, boolean](const std::string& text)
            {
                float number;
                auto& value = m_Document.Parameters[i].DefaultValue;
                if (boolean && (text == "true" || text == "false"))
                    value = text == "true";
                else if (!boolean && ParseFloat(text, number))
                    value = number;
                else
                    return;
                Changed();
                m_Refresh = false;
            }
        );
        Action(
            m_Inspector,
            "Remove parameter",
            [this, i]
            {
                const auto& name = m_Document.Parameters[i].Name;
                for (const auto& blend : m_Document.Library.BlendSpaces)
                {
                    if (blend.XParameter == name ||
                        (blend.TwoDimensional && blend.YParameter == name))
                    {
                        m_Status->SetText(
                            "Remove this parameter's blend-space bindings first."
                        );
                        return;
                    }
                }
                for (const auto& state : m_Document.Machine.States)
                {
                    for (const auto& binding : state.InputParameters)
                    {
                        if (binding == name)
                        {
                            m_Status->SetText("Remove this parameter's input bindings first.");
                            return;
                        }
                    }
                }
                for (const auto& transition : m_Document.Machine.Transitions)
                {
                    for (const auto& condition : transition.Conditions)
                    {
                        if (condition.Parameter == name)
                        {
                            m_Status->SetText("Remove this parameter's conditions first.");
                            return;
                        }
                    }
                }
                m_Document.Parameters.erase(
                    m_Document.Parameters.begin() + static_cast<std::ptrdiff_t>(i)
                );
                Changed();
            }
        );
    }
    Action(
        m_Inspector,
        "+ Float variable",
        [this]
        {
            AddParameter(false);
        }
    );
    Action(
        m_Inspector,
        "+ Bool variable",
        [this]
        {
            AddParameter(true);
        }
    );
}

void AnimationGraphPanel::RefreshSources()
{
    if (m_Document.Library.Sequences.empty() && m_Document.Library.BlendSpaces.empty())
        return;
    std::string error;
    auto sources = m_Document.Library.Build(error);
    if (!error.empty())
        return;
    m_Sources = std::move(sources);
    for (size_t i = 0;
         i < m_Document.Machine.States.size() && i < m_Document.StateSources.size();
         ++i)
    {
        const auto source = m_Sources.find(m_Document.StateSources[i]);
        if (source != m_Sources.end())
            m_Document.Machine.States[i].Node = source->second;
    }
}

void AnimationGraphPanel::RefreshPreviewInputs()
{
    m_PreviewInputs->ClearChildren();
    if (!m_Preview)
    {
        Caption(m_PreviewInputs, "Compile to edit live inputs.");
        return;
    }
    for (const auto& parameter : m_Document.Parameters)
    {
        Caption(m_PreviewInputs, parameter.Name);
        if (std::holds_alternative<bool>(parameter.DefaultValue))
        {
            const auto checkbox = CreateRef<GUI::Checkbox>();
            checkbox->SetStyle(EditorStyle::Get().Checkbox);
            checkbox->SetChecked(
                m_Preview->GetParameters().GetBool(parameter.Name).value_or(false)
            );
            checkbox->OnCheckedChanged(
                [this, name = parameter.Name](bool value)
                {
                    if (m_Preview)
                        m_Preview->GetParameters().SetBool(name, value);
                }
            );
            m_PreviewInputs->AddChild(checkbox).SetFixedSize(22);
        }
        else
        {
            Field(
                m_PreviewInputs,
                std::to_string(
                    m_Preview->GetParameters().GetFloat(parameter.Name).value_or(0)
                ),
                [this, name = parameter.Name](const std::string& text)
                {
                    float value;
                    if (m_Preview && ParseFloat(text, value))
                        m_Preview->GetParameters().SetFloat(name, value);
                }
            );
        }
    }
}

void AnimationGraphPanel::SeekPreview(float seconds)
{
    if (!m_Preview)
        Compile();
    m_Playing = false;
    if (m_Preview && m_Preview->GetBaseLayer().GetPlayer().Seek(seconds))
        m_Preview->Update(0);
}

void AnimationGraphPanel::AddParameter(bool boolean)
{
    size_t suffix = m_Document.Parameters.size() + 1;
    std::string name;
    bool exists;
    do
    {
        name = "Parameter" + std::to_string(suffix++);
        exists = false;
        for (const auto& parameter : m_Document.Parameters)
            exists = exists || parameter.Name == name;
    } while (exists);
    TAnimationParameterValue value =
        boolean ? TAnimationParameterValue(false) : TAnimationParameterValue(0.0f);
    m_Document.Parameters.push_back({name, value});
    m_ShowDefaults = true;
    Changed();
}
