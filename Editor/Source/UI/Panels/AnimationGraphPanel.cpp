#include "AnimationGraphPanel.h"
#include "../EditorStyles.h"

#include <Engine/Animation/SequenceNode.h>
#include <Engine/Animation/BlendSpaceNode.h>
#include <Engine/Animation/AnimationEvaluation.h>
#include <Engine/Font/FontManager.h>

#include <charconv>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#ifdef EE_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace
{
    constexpr glm::vec2 NodeSize{180, 74};

    /** @brief Create visible text and interaction states for graph action buttons. */
    GUI::SButtonStyle GraphButtonStyle()
    {
        const auto& chrome = EditorStyle::Get();
        auto style = chrome.PanelHeaderButton;
        style.Normal.Foreground = chrome.TextPrimary;
        style.Hovered = style.Normal;
        style.Hovered->Background.Color = {0.18f, 0.21f, 0.27f, 1};
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
        button->SetFontSize(12);
        button->OnClick(callback);
        list->AddChild(button).SetFixedSize(30).SetHorizontalAlignment(
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
        label->SetFontSize(12);
        label->SetColor(EditorStyle::Get().TextSecondary);
        list->AddChild(label).SetFixedSize(26);
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
        field->SetFontSize(13);
        field->OnChange(callback);
        list->AddChild(field).SetFixedSize(30).SetHorizontalAlignment(
            GUI::EHorizontalAlignment::Fill
        );
    }
}

/** Draws and manipulates graph nodes in a clipped, pannable canvas. */
class AnimationGraphCanvas final : public GUI::Widget
{
  public:
    /**
     * @brief Retain the document owned by the containing panel.
     * @param document Document that outlives this canvas.
     */
    explicit AnimationGraphCanvas(AnimationGraphDocument& document) : m_Document(document) {}

    /** Receives state selections; -1 clears the selection. */
    std::function<void(int)> Select;
    /** Receives transition selections. */
    std::function<void(int)> SelectTransition;
    /** Receives completed node moves. */
    std::function<void()> Edited;
    /** Receives resource selections in the pose composition view. */
    std::function<void(EAnimationAssetKind, int)> SelectAsset;
    /** Opens the connected state machine's internal graph. */
    std::function<void()> OpenMachine;

    /**
     * @brief Select the pose composition or state-machine view.
     * @param library True to show pose sources and montage actions.
     * @param kind Selected resource category.
     * @param index Selected resource index, or -1 for the graph output.
     */
    void SetLibraryView(bool library, EAnimationAssetKind kind, int index)
    {
        if (m_LibraryView == library && m_AssetKind == kind && m_AssetIndex == index)
            return;
        m_LibraryView = library;
        m_AssetKind = kind;
        m_AssetIndex = index;
        MarkRenderDirty();
    }

    /**
     * @brief Set the sampled timeline and blend coordinates for the resource detail view.
     * @param time Current preview time in seconds.
     * @param coordinates Current blend-space inputs.
     * @param blend Compiled resource used to display actual sample weights.
     */
    void SetPreview(float time, glm::vec2 coordinates, Ref<const BlendSpace> blend)
    {
        if (m_Time == time && m_Coordinates == coordinates && m_Blend == blend)
            return;
        m_Time = time;
        m_Coordinates = coordinates;
        m_Blend = std::move(blend);
        MarkRenderDirty();
    }

    /**
     * @brief Refresh graph selection and runtime highlighting.
     * @param state Selected state index.
     * @param transition Selected transition index.
     * @param active Runtime state name.
     */
    void Refresh(int state, int transition, std::string active)
    {
        if (m_Selected == state && m_SelectedTransition == transition && m_Active == active)
            return;
        m_Selected = state;
        m_SelectedTransition = transition;
        m_Active = std::move(active);
        MarkRenderDirty();
    }

    /** @brief Reset the canvas camera to the graph origin. */
    void ResetView()
    {
        m_Pan = {32, 60};
        m_Zoom = 1;
        MarkRenderDirty();
    }

    /** @brief Invalidate cached commands after a document edit. */
    void DocumentChanged() { MarkRenderDirty(); }

    /** @brief Claim mouse events for selection and canvas navigation. */
    bool CanHandleMouseInput() const override { return IsEnabled(); }

  protected:
    /**
     * @brief Use the available canvas space.
     * @param availableSize Layout constraint.
     */
    glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
    {
        return glm::min(availableSize, glm::vec2(1200, 800));
    }

    /**
     * @brief Draw nodes, directed transition paths, and the grid.
     * @param batch Destination commands.
     * @param zOrder First draw layer.
     */
    void BuildDrawCommands(GUI::RenderBatch& batch, int zOrder) override
    {
        const auto& styles = EditorStyle::Get();
        Rect(batch, m_Geometry, {0.055f, 0.065f, 0.085f, 1}, zOrder);
        if (m_LibraryView)
        {
            DrawLibrary(batch, zOrder);
            batch.Clip(m_Geometry);
            return;
        }
        const float step = 32 * m_Zoom;
        for (float x = std::fmod(m_Pan.x, step); x < m_Geometry.Size.x; x += step)
        {
            for (float y = std::fmod(m_Pan.y, step); y < m_Geometry.Size.y; y += step)
                Rect(
                    batch,
                    {m_Geometry.Position + glm::vec2(x, y), {2, 2}},
                    {0.15f, 0.17f, 0.21f, 1},
                    zOrder
                );
        }
        for (size_t i = 0; i < m_Document.Machine.Transitions.size(); ++i)
        {
            const auto [start, end] = Endpoints(i);
            const auto color = int(i) == m_SelectedTransition
                                   ? styles.Accent
                                   : GUI::SColor{0.42f, 0.49f, 0.61f, 1};
            // Orthogonal lanes separate opposite directions and show destination arrows.
            const float lane = std::min(start.y, end.y) - (28 + float(i % 4) * 18) * m_Zoom;
            Line(batch, start, {start.x, lane}, color, zOrder);
            Line(batch, {start.x, lane}, {end.x, lane}, color, zOrder);
            Line(batch, {end.x, lane}, end, color, zOrder);
            Line(batch, end + glm::vec2(-6, -9), end, color, zOrder);
            Line(batch, end + glm::vec2(6, -9), end, color, zOrder);
            const auto& transition = m_Document.Machine.Transitions[i];
            Text(
                batch,
                std::to_string(i + 1) + "  " + transition.From + " > " + transition.To,
                {(start.x + end.x) * 0.5f - 70, lane - 18},
                color,
                zOrder
            );
        }
        for (size_t i = 0; i < m_Document.Machine.States.size(); ++i)
        {
            const auto& state = m_Document.Machine.States[i];
            const auto rect = NodeRect(i);
            const bool active = state.Name == m_Active;
            const auto color = active ? GUI::SColor{0.10f, 0.32f, 0.25f, 1}
                                      : GUI::SColor{0.12f, 0.15f, 0.20f, 1};
            Rect(batch, rect, color, zOrder + 1);
            if (int(i) == m_Selected || active)
            {
                Rect(
                    batch,
                    {rect.Position, {rect.Size.x, 3}},
                    active ? GUI::SColor{0.3f, 0.9f, 0.6f, 1} : styles.Accent,
                    zOrder + 2
                );
            }
            Text(
                batch,
                state.Name,
                rect.Position + glm::vec2(12, 12),
                styles.TextPrimary,
                zOrder + 2
            );
            const std::string entry =
                state.Name == m_Document.Machine.InitialState ? "ENTRY  /  " : "";
            Text(
                batch,
                entry + (state.Looping ? "Loop" : "Once"),
                rect.Position + glm::vec2(12, 42) * m_Zoom,
                styles.TextSecondary,
                zOrder + 2
            );
        }
        batch.Clip(m_Geometry);
    }

    /**
     * @brief Select and capture a node drag or background pan.
     * @param event Pointer press in GUI coordinates.
     */
    GUI::SInputReply HandleMouseDown(const MouseButtonPressedEvent& event) override
    {
        m_Pressed = true;
        m_LastMouse = {event.GetX(), event.GetY()};
        m_Drag = -1;
        m_Moved = false;
        if (m_LibraryView)
        {
            const auto assets = Assets();
            for (size_t i = assets.size(); i > 0; --i)
            {
                const auto& asset = assets[i - 1];
                if (AssetRect(*asset.Position).Contains(m_LastMouse))
                {
                    m_Drag = int(i - 1);
                    if (SelectAsset)
                        SelectAsset(asset.Kind, int(asset.Index));
                    return GUI::SInputReply::HandledAndCaptured();
                }
            }
            if (AssetRect({520, 110}).Contains(m_LastMouse))
            {
                if (OpenMachine)
                    OpenMachine();
            }
            else if (SelectAsset)
                SelectAsset(EAnimationAssetKind::Sequence, -1);
            return GUI::SInputReply::HandledAndCaptured();
        }
        if (event.GetMouseButton() == 0)
        {
            for (size_t i = m_Document.Positions.size(); i > 0; --i)
            {
                if (NodeRect(i - 1).Contains(m_LastMouse))
                {
                    m_Drag = int(i - 1);
                    if (Select)
                        Select(m_Drag);
                    return GUI::SInputReply::HandledAndCaptured();
                }
            }
            for (size_t i = 0; i < m_Document.Machine.Transitions.size(); ++i)
            {
                const auto [start, end] = Endpoints(i);
                const float lane =
                    std::min(start.y, end.y) - (28 + float(i % 4) * 18) * m_Zoom;
                const GUI::SRect hit{
                    {std::min(start.x, end.x) - 8, lane - 20},
                    {std::abs(end.x - start.x) + 16, 28}
                };
                if (hit.Contains(m_LastMouse))
                {
                    if (SelectTransition)
                        SelectTransition(int(i));
                    m_Pressed = false;
                    return GUI::SInputReply::Handled();
                }
            }
            if (Select)
                Select(-1);
        }
        return GUI::SInputReply::HandledAndCaptured();
    }

    /**
     * @brief Move the captured node or pan the camera.
     * @param event Current pointer position.
     */
    GUI::SInputReply HandleMouseMove(const MouseMovedEvent& event) override
    {
        const glm::vec2 point{event.GetX(), event.GetY()};
        if (m_Pressed)
        {
            const auto delta = point - m_LastMouse;
            if (m_Drag >= 0 && m_LibraryView)
            {
                const auto assets = Assets();
                if (size_t(m_Drag) < assets.size())
                    *assets[m_Drag].Position += delta / m_Zoom;
            }
            else if (m_Drag >= 0)
                m_Document.Positions[m_Drag] += delta / m_Zoom;
            else
                m_Pan += delta;
            m_Moved = m_Moved || glm::length(delta) > 0;
            MarkRenderDirty();
        }
        m_LastMouse = point;
        return GUI::SInputReply::Handled();
    }

    /**
     * @brief Commit a completed node drag.
     * @param event Released pointer button.
     */
    GUI::SInputReply HandleMouseUp(const MouseButtonReleasedEvent& event) override
    {
        m_Pressed = false;
        if (m_Drag >= 0 && m_Moved && Edited)
            Edited();
        m_Drag = -1;
        return GUI::SInputReply::Handled();
    }

    /**
     * @brief Zoom around the last pointer position.
     * @param event Scroll-wheel delta.
     */
    GUI::SInputReply HandleMouseScrolled(const MouseScrolledEvent& event) override
    {
        const auto old = m_Zoom;
        m_Zoom = std::clamp(m_Zoom * std::pow(1.1f, event.GetOffsetY()), 0.4f, 1.8f);
        const auto local = m_LastMouse - m_Geometry.Position;
        m_Pan = local - (local - m_Pan) * (m_Zoom / old);
        MarkRenderDirty();
        return GUI::SInputReply::Handled();
    }

  private:
    /** Describes a resource node while the document retains its position. */
    struct SAssetView
    {
        EAnimationAssetKind Kind;
        size_t Index;
        std::string Name;
        std::string Type;
        glm::vec2* Position;
    };

    /** @brief Enumerate editable pose sources and montage actions in draw order. */
    std::vector<SAssetView> Assets()
    {
        std::vector<SAssetView> assets;
        auto& library = m_Document.Library;
        for (size_t i = 0; i < library.Sequences.size(); ++i)
            assets.push_back(
                {EAnimationAssetKind::Sequence,
                 i,
                 library.Sequences[i].Name,
                 "Sequence",
                 &library.Sequences[i].Position}
            );
        for (size_t i = 0; i < library.BlendSpaces.size(); ++i)
            assets.push_back(
                {EAnimationAssetKind::BlendSpace,
                 i,
                 library.BlendSpaces[i].Name,
                 library.BlendSpaces[i].TwoDimensional ? "Blend Space 2D" : "Blend Space 1D",
                 &library.BlendSpaces[i].Position}
            );
        for (size_t i = 0; i < library.Montages.size(); ++i)
            assets.push_back(
                {EAnimationAssetKind::Montage,
                 i,
                 library.Montages[i].Name,
                 "Montage / layer action",
                 &library.Montages[i].Position}
            );
        return assets;
    }

    /**
     * @brief Convert an authored resource position to its canvas rectangle.
     * @param position Node position in graph units.
     */
    GUI::SRect AssetRect(glm::vec2 position) const
    {
        return {m_Geometry.Position + m_Pan + position * m_Zoom, NodeSize * m_Zoom};
    }

    /**
     * @brief Draw typed resource nodes and their pose or sequence connections.
     * @param batch Destination commands.
     * @param layer First draw layer.
     */
    void DrawLibrary(GUI::RenderBatch& batch, int layer)
    {
        const auto assets = Assets();
        const auto& styles = EditorStyle::Get();
        const auto socket = [this, &assets](const std::string& name, bool output)
        {
            for (const auto& asset : assets)
            {
                if (asset.Name == name)
                {
                    const auto rect = AssetRect(*asset.Position);
                    return rect.Position +
                           glm::vec2(output ? rect.Size.x : 0, rect.Size.y / 2);
                }
            }
            const auto rect = AssetRect({520, 110});
            return rect.Position + glm::vec2(rect.Size.x, rect.Size.y / 2);
        };
        for (const auto& blend : m_Document.Library.BlendSpaces)
            for (const auto& sample : blend.Samples)
                Line(
                    batch,
                    socket(sample.Sequence, true),
                    socket(blend.Name, false),
                    styles.Accent,
                    layer
                );
        for (const auto& montage : m_Document.Library.Montages)
            Line(
                batch,
                socket(montage.Sequence, true),
                socket(montage.Name, false),
                {0.8f, 0.55f, 0.25f, 1},
                layer
            );
        const auto machine = AssetRect({520, 110});
        for (const auto& source : m_Document.StateSources)
            Line(
                batch,
                socket(source, true),
                machine.Position + glm::vec2(0, machine.Size.y / 2),
                {0.3f, 0.45f, 0.45f, 1},
                layer
            );
        const auto output = AssetRect({760, 110});
        Line(
            batch,
            socket(m_Document.OutputSource, true),
            output.Position + glm::vec2(0, output.Size.y / 2),
            styles.Accent,
            layer
        );
        for (const auto& asset : assets)
        {
            const auto rect = AssetRect(*asset.Position);
            const bool selected =
                asset.Kind == m_AssetKind && int(asset.Index) == m_AssetIndex;
            Rect(batch, rect, {0.12f, 0.15f, 0.20f, 1}, layer + 1);
            Rect(
                batch,
                {rect.Position, {rect.Size.x, 3}},
                selected ? styles.Accent : GUI::SColor{0.3f, 0.4f, 0.5f, 1},
                layer + 2
            );
            Text(
                batch,
                asset.Name,
                rect.Position + glm::vec2(10, 10),
                styles.TextPrimary,
                layer + 2
            );
            Text(
                batch,
                asset.Type,
                rect.Position + glm::vec2(10, 42),
                styles.TextSecondary,
                layer + 2
            );
        }
        for (const auto& entry :
             {std::pair{glm::vec2(520, 110), "State Machine"},
              std::pair{glm::vec2(760, 110), "Output Pose"}})
        {
            const auto rect = AssetRect(entry.first);
            Rect(batch, rect, {0.12f, 0.23f, 0.23f, 1}, layer + 1);
            Text(
                batch,
                entry.second,
                rect.Position + glm::vec2(10, 12),
                styles.TextPrimary,
                layer + 2
            );
            Text(
                batch,
                entry.first.x == 520 ? "Click to edit states" : "Compiled graph",
                rect.Position + glm::vec2(10, 42),
                styles.TextSecondary,
                layer + 2
            );
        }
        DrawDetails(batch, layer + 3);
    }

    /**
     * @brief Draw the selected resource timeline or blend-space sample plane.
     * @param batch Destination commands.
     * @param layer Draw layer above graph connections.
     */
    void DrawDetails(GUI::RenderBatch& batch, int layer)
    {
        if (m_AssetIndex < 0)
            return;
        const auto& library = m_Document.Library;
        const auto origin = m_Geometry.Position + glm::vec2(20, m_Geometry.Size.y - 155);
        const auto width = std::max(100.0f, m_Geometry.Size.x - 40);
        Rect(batch, {origin, {width, 140}}, {0.08f, 0.10f, 0.14f, 0.98f}, layer);
        const auto& styles = EditorStyle::Get();
        if (m_AssetKind == EAnimationAssetKind::BlendSpace &&
            size_t(m_AssetIndex) < library.BlendSpaces.size())
        {
            const auto& blend = library.BlendSpaces[m_AssetIndex];
            Text(
                batch,
                blend.Name + " / samples and runtime weights",
                origin + glm::vec2(8),
                styles.TextPrimary,
                layer
            );
            glm::vec2 lower{0}, upper{1};
            for (const auto& sample : blend.Samples)
            {
                lower = glm::min(lower, sample.Position);
                upper = glm::max(upper, sample.Position);
            }
            const auto project = [&](glm::vec2 point)
            {
                const auto normalized =
                    (point - lower) / glm::max(upper - lower, glm::vec2(0.01f));
                return origin + glm::vec2(30, 105) +
                       normalized * glm::vec2(
                                        std::min(width - 130, 400.0f),
                                        blend.TwoDimensional ? -65 : 0
                                    );
            };
            for (const auto& sample : blend.Samples)
            {
                const auto point = project(sample.Position);
                Rect(batch, {point, {8, 8}}, styles.Accent, layer);
                Text(
                    batch,
                    sample.Sequence,
                    point + glm::vec2(10, -12),
                    styles.TextSecondary,
                    layer
                );
            }
            Rect(
                batch,
                {project(m_Coordinates) - glm::vec2(3), {14, 14}},
                {0.3f, 0.9f, 0.6f, 1},
                layer
            );
            if (m_Blend)
            {
                const std::array values{m_Coordinates.x, m_Coordinates.y};
                std::vector<SSequenceContribution> contributions;
                if (m_Blend->CalculateContributions(
                        std::span(values).first(m_Blend->GetDimensionCount()),
                        contributions
                    ))
                {
                    float y = 35;
                    for (const auto& contribution : contributions)
                    {
                        Text(
                            batch,
                            contribution.Sequence->GetName() + ": " +
                                std::to_string(contribution.Weight),
                            origin + glm::vec2(width - 240, y),
                            styles.TextPrimary,
                            layer
                        );
                        y += 22;
                    }
                }
            }
            return;
        }
        float duration = 0;
        if (m_AssetKind == EAnimationAssetKind::Sequence &&
            size_t(m_AssetIndex) < library.Sequences.size())
        {
            const auto& clip = library.Sequences[m_AssetIndex];
            duration = clip.Duration;
            Text(
                batch,
                clip.Name + " / " + std::to_string(duration) + " seconds / " +
                    std::to_string(clip.Tracks.size()) + " tracks",
                origin + glm::vec2(8),
                styles.TextPrimary,
                layer
            );
            for (const auto& marker : clip.Markers)
            {
                const float x = marker.Time / std::max(duration, 0.001f) * (width - 30);
                Rect(batch, {origin + glm::vec2(x + 15, 45), {2, 70}}, styles.Accent, layer);
                Text(
                    batch,
                    marker.Name,
                    origin + glm::vec2(x + 20, 45),
                    styles.TextSecondary,
                    layer
                );
            }
            for (const auto& notify : clip.Notifies)
            {
                const float x = notify.Time / std::max(duration, 0.001f) * (width - 30);
                Rect(
                    batch,
                    {origin + glm::vec2(x + 15, 90),
                     {std::max(
                          4.0f,
                          notify.Duration / std::max(duration, 0.001f) * (width - 30)
                      ),
                      15}},
                    {0.8f, 0.55f, 0.25f, 1},
                    layer
                );
                Text(
                    batch,
                    notify.Name,
                    origin + glm::vec2(x + 20, 110),
                    styles.TextSecondary,
                    layer
                );
            }
        }
        if (m_AssetKind == EAnimationAssetKind::Montage &&
            size_t(m_AssetIndex) < library.Montages.size())
        {
            const auto& montage = library.Montages[m_AssetIndex];
            for (const auto& clip : library.Sequences)
                if (clip.Name == montage.Sequence)
                    duration = clip.Duration;
            Text(
                batch,
                montage.Name + " / layer action / section timeline",
                origin + glm::vec2(8),
                styles.TextPrimary,
                layer
            );
            for (const auto& section : montage.Sections)
            {
                const float x = section.StartTime / std::max(duration, 0.001f) * (width - 30);
                Rect(batch, {origin + glm::vec2(x + 15, 45), {2, 70}}, styles.Accent, layer);
                Text(
                    batch,
                    section.Name + " > " + section.NextSection.value_or("next"),
                    origin + glm::vec2(x + 20, 65),
                    styles.TextSecondary,
                    layer
                );
            }
        }
        const float cursor =
            std::clamp(m_Time / std::max(duration, 0.001f), 0.0f, 1.0f) * (width - 30);
        Rect(
            batch,
            {origin + glm::vec2(cursor + 15, 30), {2, 100}},
            {0.3f, 0.9f, 0.6f, 1},
            layer
        );
    }
    /**
     * @brief Get the screen rectangle of a state.
     * @param index State index.
     */
    GUI::SRect NodeRect(size_t index) const
    {
        return {
            m_Geometry.Position + m_Pan + m_Document.Positions[index] * m_Zoom,
            NodeSize * m_Zoom
        };
    }

    /**
     * @brief Find a transition's source and destination sockets.
     * @param index Transition index.
     */
    std::pair<glm::vec2, glm::vec2> Endpoints(size_t index) const
    {
        const auto& transition = m_Document.Machine.Transitions[index];
        glm::vec2 start{}, end{};
        for (size_t i = 0; i < m_Document.Machine.States.size(); ++i)
        {
            const auto rect = NodeRect(i);
            if (m_Document.Machine.States[i].Name == transition.From)
                start = rect.Position + glm::vec2(rect.Size.x * 0.7f, 0);
            if (m_Document.Machine.States[i].Name == transition.To)
                end = rect.Position + glm::vec2(rect.Size.x * 0.3f, 0);
        }
        return {start, end};
    }

    /**
     * @brief Draw a clipped rectangle.
     * @param batch Destination batch.
     * @param rect Screen rectangle.
     * @param color Fill color.
     * @param layer Draw layer.
     */
    void
    Rect(GUI::RenderBatch& batch, const GUI::SRect& rect, GUI::SColor color, int layer) const
    {
        batch.AddRect(rect, color, {}, {}, {}, {}, layer);
    }

    /**
     * @brief Draw a directed path segment with small square samples.
     * @param batch Destination batch.
     * @param from First endpoint.
     * @param to Second endpoint.
     * @param color Path color.
     * @param layer Draw layer.
     */
    void Line(
        GUI::RenderBatch& batch,
        glm::vec2 from,
        glm::vec2 to,
        GUI::SColor color,
        int layer
    ) const
    {
        if (from.x == to.x || from.y == to.y)
        {
            Rect(
                batch,
                {glm::min(from, to), glm::abs(to - from) + glm::vec2(2)},
                color,
                layer
            );
            return;
        }
        const int steps = int(std::clamp(glm::length(to - from) / 2, 1.0f, 2000.0f));
        for (int i = 0; i <= steps; ++i)
            Rect(batch, {glm::mix(from, to, float(i) / float(steps)), {2, 2}}, color, layer);
    }

    /**
     * @brief Draw a clipped canvas label.
     * @param batch Destination batch.
     * @param text Label text.
     * @param position Screen position.
     * @param color Text color.
     * @param layer Draw layer.
     */
    void Text(
        GUI::RenderBatch& batch,
        const std::string& text,
        glm::vec2 position,
        GUI::SColor color,
        int layer
    ) const
    {
        batch.AddText(
            text,
            {position, {230 * m_Zoom, 24 * m_Zoom}},
            FontManager::GetDefaultFont(),
            13 * m_Zoom,
            color,
            layer
        );
    }

    AnimationGraphDocument& m_Document;
    glm::vec2 m_Pan{32, 60};
    glm::vec2 m_LastMouse{};
    float m_Zoom = 1;
    int m_Selected = -1;
    int m_SelectedTransition = -1;
    int m_Drag = -1;
    bool m_Moved = false;
    std::string m_Active;
    bool m_LibraryView = true;
    EAnimationAssetKind m_AssetKind = EAnimationAssetKind::Sequence;
    int m_AssetIndex = -1;
    float m_Time = 0;
    glm::vec2 m_Coordinates{};
    Ref<const BlendSpace> m_Blend;
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
        sequence.Position = {0, height * 200};
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
         {260, 150}}
    };
    m_Document.Library.Montages = {
        {"Action",
         "Run",
         0.15f,
         0.15f,
         true,
         {{"Start", 0}, {"Action", 0.4f, std::string("Action")}, {"End", 0.8f}},
         {520, 210}}
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
    m_State = -1;
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
    m_LibraryView = !m_Document.Library.Sequences.empty();
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
    root->SetStyle(EditorStyle::Get().Panel);
    m_Canvas = CreateRef<AnimationGraphCanvas>(m_Document);
    root->AddChild(m_Canvas)
        .SetAnchors(GUI::SAnchors::StretchAll())
        .SetOffsets(0, 82, -300, -32);
    m_Canvas->Select = [this](int index)
    {
        if (m_ConnectFrom >= 0 && index >= 0 && m_ConnectFrom != index)
        {
            m_Document.Machine.Transitions.push_back(
                {m_Document.Machine.States[m_ConnectFrom].Name,
                 m_Document.Machine.States[index].Name,
                 {}}
            );
            Changed();
        }
        m_ConnectFrom = -1;
        m_State = index;
        m_Transition = -1;
        m_Refresh = true;
    };
    m_Canvas->SelectTransition = [this](int index)
    {
        m_State = -1;
        m_Transition = index;
        m_ConnectFrom = -1;
        m_Refresh = true;
    };
    m_Canvas->Edited = [this]
    {
        Changed();
    };
    m_Canvas->SelectAsset = [this](EAnimationAssetKind kind, int index)
    {
        m_AssetKind = kind;
        m_Asset = index;
        m_State = -1;
        m_Transition = -1;
        m_Refresh = true;
    };
    m_Canvas->OpenMachine = [this]
    {
        m_LibraryView = false;
        m_State = m_Document.Machine.States.empty() ? -1 : 0;
        m_Refresh = true;
    };

    const auto toolbar = CreateRef<GUI::HorizontalBox>();
    toolbar->SetStyle(EditorStyle::Get().Toolbar);
    root->AddChild(toolbar).SetAnchors({0, 0, 1, 0}).SetSize({0, 40});
    const auto addButton = [&toolbar](const std::string& title, auto callback)
    {
        const auto button = CreateRef<GUI::Button>(title);
        button->SetStyle(GraphButtonStyle());
        button->SetFontSize(12);
        button->OnClick(callback);
        toolbar->AddChild(button).SetFixedSize(100);
    };
    addButton(
        "Pose / States",
        [this]
        {
            m_LibraryView = !m_LibraryView;
            m_Refresh = true;
        }
    );
    addButton(
        "+ Sequence",
        [this]
        {
            AddAsset(EAnimationAssetKind::Sequence);
        }
    );
    addButton(
        "+ Blend 1D",
        [this]
        {
            AddAsset(EAnimationAssetKind::BlendSpace);
        }
    );
    addButton(
        "+ Blend 2D",
        [this]
        {
            AddAsset(EAnimationAssetKind::BlendSpace, true);
        }
    );
    addButton(
        "+ Montage",
        [this]
        {
            AddAsset(EAnimationAssetKind::Montage);
        }
    );
    addButton(
        "+ State",
        [this]
        {
            if (m_Sources.empty())
            {
                m_Status->SetText("Register an animation source before adding states.");
                return;
            }
            size_t suffix = m_Document.Machine.States.size() + 1;
            std::string name;
            bool exists;
            do
            {
                name = "State " + std::to_string(suffix++);
                exists = false;
                for (const auto& state : m_Document.Machine.States)
                    exists = exists || state.Name == name;
            } while (exists);
            m_Document.Machine.States.push_back({name, m_Sources.begin()->second});
            m_Document.StateSources.push_back(m_Sources.begin()->first);
            m_Document.Machine.States.back().InputParameters =
                m_Document.Library.Bindings(m_Sources.begin()->first);
            m_Document.Positions.push_back({80, 260});
            if (m_Document.Machine.InitialState.empty())
                m_Document.Machine.InitialState = name;
            m_State = int(m_Document.Machine.States.size() - 1);
            m_Transition = -1;
            m_LibraryView = false;
            Changed();
        }
    );
    addButton(
        "Compile",
        [this]
        {
            Compile();
        }
    );
    addButton(
        "Play / Pause",
        [this]
        {
            if (!m_Preview)
                Compile();
            m_Playing = m_Preview && !m_Playing;
            m_Refresh = true;
        }
    );
    addButton(
        "Undo",
        [this]
        {
            History(false);
        }
    );
    addButton(
        "Redo",
        [this]
        {
            History(true);
        }
    );
    addButton(
        "Reset view",
        [this]
        {
            m_Canvas->ResetView();
        }
    );

    addButton(
        "Play Action",
        [this]
        {
            if (m_AssetKind == EAnimationAssetKind::Montage && m_Asset >= 0)
                PlayMontage(size_t(m_Asset));
            else
                m_Status->SetText("Select a montage node, then choose Play Action.");
        }
    );

    const auto filebar = CreateRef<GUI::HorizontalBox>();
    root->AddChild(filebar).SetAnchors({0, 0, 1, 0}).SetPosition({0, 42}).SetSize({0, 36});
    m_Path = CreateRef<GUI::TextField>("AnimationGraph.animgraph");
    m_Path->SetStyle(EditorStyle::Get().TextField);
    filebar->AddChild(m_Path).SetFillSize();
    for (const bool save : {true, false})
    {
        const auto button = CreateRef<GUI::Button>(save ? "Save" : "Load");
        button->SetStyle(GraphButtonStyle());
        button->OnClick(
            [this, save]
            {
                FileAction(save);
            }
        );
        filebar->AddChild(button).SetFixedSize(90);
    }

    const auto scroll = CreateRef<GUI::ScrollBox>();
    scroll->SetStyle(EditorStyle::Get().ScrollBar);
    root->AddChild(scroll)
        .SetAnchors({1, 0, 1, 1})
        .SetPosition({-296, 0})
        .SetSize({292, 0})
        .SetOffsets(0, 82, 0, -32);
    m_Inspector = CreateRef<GUI::VerticalBox>();
    m_Inspector->SetPadding(GUI::SPadding(12));
    scroll->SetContent(m_Inspector);

    m_Status =
        CreateRef<GUI::TextBlock>("Select a typed resource. Compile, then Play to preview.");
    m_Status->SetFontSize(12);
    m_Status->SetColor(EditorStyle::Get().TextPrimary);
    root->AddChild(m_Status)
        .SetAnchors({0, 1, 1, 1})
        .SetPosition({0, -28})
        .SetSize({0, 24})
        .SetOffsets(8, 0, -8, 0);
    RefreshInspector();
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
    m_State = -1;
    m_Transition = -1;
    m_ConnectFrom = -1;
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
        m_State = -1;
        m_Transition = -1;
        m_ConnectFrom = -1;
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
    if (m_Preview && m_Playing)
    {
        if (!m_Preview->Update(std::min(float(frameTime), 0.1f)))
        {
            m_Playing = false;
            m_Status->SetText("Runtime preview failed to advance.");
        }
    }
    if (m_Canvas)
    {
        const auto active = m_Preview ? std::string(m_Preview->GetCurrentState()) : "";
        m_Canvas->Refresh(m_State, m_Transition, active);
        m_Canvas->SetLibraryView(m_LibraryView, m_AssetKind, m_Asset);
        glm::vec2 coordinates{};
        Ref<const BlendSpace> blend;
        if (m_AssetKind == EAnimationAssetKind::BlendSpace && m_Asset >= 0 &&
            size_t(m_Asset) < m_Document.Library.BlendSpaces.size())
        {
            const auto& asset = m_Document.Library.BlendSpaces[m_Asset];
            if (m_Preview)
            {
                coordinates.x =
                    m_Preview->GetParameters().GetFloat(asset.XParameter).value_or(0);
                coordinates.y =
                    m_Preview->GetParameters().GetFloat(asset.YParameter).value_or(0);
            }
            const auto source = m_Sources.find(asset.Name);
            if (source != m_Sources.end())
            {
                const auto node =
                    std::dynamic_pointer_cast<const BlendSpaceNode>(source->second);
                if (node)
                    blend = node->GetBlendSpace();
            }
        }
        float time = m_Preview ? m_Preview->GetBaseLayer().GetPlayer().GetTime() : 0;
        if (m_Preview && m_AssetKind == EAnimationAssetKind::Montage)
            time = float(m_Preview->GetBaseLayer().GetPlayer().GetMontageTime());
        m_Canvas->SetPreview(time, coordinates, blend);
        if (m_PlaybackStatus && m_Preview)
        {
            const auto& player = m_Preview->GetBaseLayer().GetPlayer();
            const auto& transforms = m_Preview->GetLocalPose().LocalTransforms;
            const float root = transforms.empty() ? 0 : transforms.front().Translation.y;
            m_PlaybackStatus->SetText(
                "t=" + std::to_string(time) + " root Y=" + std::to_string(root) +
                " section=" + std::string(player.GetCurrentMontageSection())
            );
        }
    }
}

void AnimationGraphPanel::RefreshInspector()
{
    m_Inspector->ClearChildren();
    m_PlaybackStatus.reset();
    Caption(
        m_Inspector,
        m_LibraryView ? "ANIMATION GRAPH / POSE SOURCES" : "ANIMATION GRAPH / STATE MACHINE"
    );
    Caption(m_Inspector, "Drag nodes. Drag background to pan.");
    Caption(m_Inspector, "Scroll to zoom. Click a wire to inspect.");
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
    if (m_LibraryView)
    {
        if (m_Preview)
        {
            Caption(m_Inspector, m_Playing ? "LIVE INPUTS / PLAYING" : "LIVE INPUTS / PAUSED");
            for (const auto& parameter : m_Document.Parameters)
            {
                if (std::holds_alternative<float>(parameter.DefaultValue))
                {
                    Caption(m_Inspector, parameter.Name);
                    Field(
                        m_Inspector,
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
                else
                {
                    Action(
                        m_Inspector,
                        "Toggle live " + parameter.Name,
                        [this, name = parameter.Name]
                        {
                            if (!m_Preview)
                                return;
                            auto& parameters = m_Preview->GetParameters();
                            parameters.SetBool(
                                name,
                                !parameters.GetBool(name).value_or(false)
                            );
                        }
                    );
                }
            }
        }
        BuildAssetInspector();
    }
    if (!m_LibraryView && m_State >= 0 && size_t(m_State) < m_Document.Machine.States.size())
    {
        const auto& state = m_Document.Machine.States[m_State];
        Caption(m_Inspector, "STATE NAME");
        Field(
            m_Inspector,
            state.Name,
            [this](const std::string& value)
            {
                if (m_Document.RenameState(m_State, value))
                {
                    Changed();
                    m_Refresh = false;
                }
                else
                    m_Status->SetText("State name must be nonempty and unique.");
            }
        );
        Action(
            m_Inspector,
            "Set as entry state",
            [this]
            {
                m_Document.Machine.InitialState = m_Document.Machine.States[m_State].Name;
                Changed();
            }
        );
        Action(
            m_Inspector,
            state.Looping ? "Playback: Loop" : "Playback: Once",
            [this]
            {
                auto& looping = m_Document.Machine.States[m_State].Looping;
                looping = !looping;
                Changed();
            }
        );
        Caption(m_Inspector, "REGISTERED SOURCES");
        for (const auto& [name, source] : m_Sources)
        {
            Action(
                m_Inspector,
                (source == state.Node ? "* " : "") + name,
                [this, source]
                {
                    if (!source)
                        return;
                    m_Document.Machine.States[m_State].Node = source;
                    if (m_Document.StateSources.size() < m_Document.Machine.States.size())
                        m_Document.StateSources.resize(m_Document.Machine.States.size());
                    for (const auto& [name, registered] : m_Sources)
                        if (registered == source)
                            m_Document.StateSources[m_State] = name;
                    m_Document.Machine.States[m_State].InputParameters =
                        m_Document.Library.Bindings(m_Document.StateSources[m_State]);
                    Changed();
                }
            );
        }
        for (size_t i = 0; i < state.InputParameters.size(); ++i)
        {
            Caption(m_Inspector, "Input " + std::to_string(i + 1) + " / parameter name");
            Field(
                m_Inspector,
                state.InputParameters[i],
                [this, i](const std::string& value)
                {
                    m_Document.Machine.States[m_State].InputParameters[i] = value;
                    Changed();
                    m_Refresh = false;
                }
            );
        }
        Action(
            m_Inspector,
            "Connect to... (then click a state)",
            [this]
            {
                m_ConnectFrom = m_State;
                m_Status->SetText("Select a destination state. Background click cancels.");
            }
        );
        Action(
            m_Inspector,
            "Delete state",
            [this]
            {
                m_Document.RemoveState(size_t(m_State));
                m_State = -1;
                m_ConnectFrom = -1;
                Changed();
            }
        );
    }
    if (m_Transition >= 0 && size_t(m_Transition) < m_Document.Machine.Transitions.size())
    {
        const auto& transition = m_Document.Machine.Transitions[m_Transition];
        Caption(m_Inspector, transition.From + " > " + transition.To);
        Caption(m_Inspector, "Fade duration (seconds)");
        Field(
            m_Inspector,
            std::to_string(transition.FadeDuration),
            [this](const std::string& text)
            {
                float value;
                if (!ParseFloat(text, value) || value < 0)
                {
                    m_Status->SetText("Fade must be a finite nonnegative number.");
                    return;
                }
                m_Document.Machine.Transitions[m_Transition].FadeDuration = value;
                Changed();
                m_Refresh = false;
            }
        );
        Action(
            m_Inspector,
            transition.WaitForCompletion ? "Wait for end: Yes" : "Wait for end: No",
            [this]
            {
                auto& wait = m_Document.Machine.Transitions[m_Transition].WaitForCompletion;
                wait = !wait;
                Changed();
            }
        );
        Caption(m_Inspector, "CONDITIONS (all must match)");
        for (size_t i = 0; i < transition.Conditions.size(); ++i)
        {
            const auto& condition = transition.Conditions[i];
            Field(
                m_Inspector,
                condition.Parameter,
                [this, i](const std::string& value)
                {
                    m_Document.Machine.Transitions[m_Transition].Conditions[i].Parameter =
                        value;
                    Changed();
                    m_Refresh = false;
                }
            );
            constexpr const char* comparisons[] = {"==", "!=", "<", "<=", ">", ">="};
            Action(
                m_Inspector,
                comparisons[int(condition.Comparison)],
                [this, i]
                {
                    auto& value = m_Document.Machine.Transitions[m_Transition].Conditions[i];
                    value.Comparison = EAnimationComparison((int(value.Comparison) + 1) % 6);
                    Changed();
                }
            );
            const bool boolean = std::holds_alternative<bool>(condition.Value);
            const auto text = boolean ? (std::get<bool>(condition.Value) ? "true" : "false")
                                      : std::to_string(std::get<float>(condition.Value));
            Field(
                m_Inspector,
                text,
                [this, i, boolean](const std::string& value)
                {
                    auto& conditionValue =
                        m_Document.Machine.Transitions[m_Transition].Conditions[i].Value;
                    float number;
                    if (boolean && (value == "true" || value == "false"))
                        conditionValue = value == "true";
                    else if (!boolean && ParseFloat(value, number))
                        conditionValue = number;
                    else
                    {
                        m_Status->SetText(
                            "Enter a finite number, or true / false for a bool."
                        );
                        return;
                    }
                    Changed();
                    m_Refresh = false;
                }
            );
            Action(
                m_Inspector,
                "Remove condition",
                [this, i]
                {
                    auto& conditions = m_Document.Machine.Transitions[m_Transition].Conditions;
                    conditions.erase(conditions.begin() + static_cast<std::ptrdiff_t>(i));
                    Changed();
                }
            );
        }
        for (const auto& parameter : m_Document.Parameters)
        {
            Action(
                m_Inspector,
                "+ Condition: " + parameter.Name,
                [this, parameter]
                {
                    m_Document.Machine.Transitions[m_Transition].Conditions.push_back(
                        {parameter.Name, EAnimationComparison::Equal, parameter.DefaultValue}
                    );
                    Changed();
                }
            );
        }
        Action(
            m_Inspector,
            "Move earlier (higher priority)",
            [this]
            {
                if (m_Transition == 0)
                    return;
                auto& transitions = m_Document.Machine.Transitions;
                std::swap(transitions[m_Transition], transitions[m_Transition - 1]);
                --m_Transition;
                Changed();
            }
        );
        Action(
            m_Inspector,
            "Delete transition",
            [this]
            {
                auto& transitions = m_Document.Machine.Transitions;
                transitions.erase(transitions.begin() + m_Transition);
                m_Transition = -1;
                Changed();
            }
        );
    }
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
    for (const bool boolean : {false, true})
    {
        Action(
            m_Inspector,
            boolean ? "+ Bool parameter" : "+ Float parameter",
            [this, boolean]
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
                Changed();
            }
        );
    }
    if (m_Preview && !m_LibraryView)
    {
        Caption(m_Inspector, m_Playing ? "LIVE PREVIEW / PLAYING" : "LIVE PREVIEW / PAUSED");
        for (const auto& parameter : m_Document.Parameters)
        {
            Caption(m_Inspector, parameter.Name + " (live)");
            if (std::holds_alternative<bool>(parameter.DefaultValue))
            {
                Action(
                    m_Inspector,
                    "Toggle " + parameter.Name,
                    [this, name = parameter.Name]
                    {
                        if (!m_Preview)
                            return;
                        auto& values = m_Preview->GetParameters();
                        values.SetBool(name, !values.GetBool(name).value_or(false));
                    }
                );
            }
            else
            {
                Field(
                    m_Inspector,
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

void AnimationGraphPanel::AddAsset(EAnimationAssetKind kind, bool twoDimensional)
{
    auto& library = m_Document.Library;
    size_t suffix = 1;
    std::string name;
    bool exists;
    do
    {
        name = (kind == EAnimationAssetKind::Sequence     ? "Sequence "
                : kind == EAnimationAssetKind::BlendSpace ? "Blend "
                                                          : "Montage ") +
               std::to_string(suffix++);
        exists = false;
        for (const auto& entry : library.Sequences)
            exists = exists || entry.Name == name;
        for (const auto& entry : library.BlendSpaces)
            exists = exists || entry.Name == name;
        for (const auto& entry : library.Montages)
            exists = exists || entry.Name == name;
    } while (exists);
    if (kind == EAnimationAssetKind::Sequence)
    {
        SAnimationSequenceAsset asset;
        asset.Name = name;
        asset.Skeleton = library.Sequences.empty()
                             ? Skeleton::Create("Graph Skeleton", {{"Root"}})
                             : library.Sequences.front().Skeleton;
        asset.Position = {0, 100.0f * float(library.Sequences.size())};
        library.Sequences.push_back(asset);
        m_Asset = int(library.Sequences.size() - 1);
    }
    else if (!library.Sequences.empty() && kind == EAnimationAssetKind::BlendSpace)
    {
        SAnimationBlendAsset asset;
        asset.Name = name;
        asset.TwoDimensional = twoDimensional;
        const auto& clip = library.Sequences.front().Name;
        asset.Samples = {{clip, {0, 0}}, {clip, {1, 0}}};
        if (twoDimensional)
            asset.Samples.push_back({clip, {0, 1}});
        asset.Position = {260, 100.0f * float(library.BlendSpaces.size())};
        library.BlendSpaces.push_back(asset);
        m_Asset = int(library.BlendSpaces.size() - 1);
        for (const auto& parameter : {asset.XParameter, asset.YParameter})
        {
            bool found = false;
            for (const auto& existing : m_Document.Parameters)
                found = found || existing.Name == parameter;
            if (!found)
                m_Document.Parameters.push_back({parameter, 0.0f});
        }
    }
    else if (!library.Sequences.empty() && kind == EAnimationAssetKind::Montage)
    {
        SAnimationMontageAsset asset;
        asset.Name = name;
        asset.Sequence = library.Sequences.front().Name;
        asset.Sections = {{"Default", 0}};
        library.Montages.push_back(asset);
        m_Asset = int(library.Montages.size() - 1);
    }
    else
    {
        m_Status->SetText("Create a sequence before adding a blend space or montage.");
        return;
    }
    m_AssetKind = kind;
    m_LibraryView = true;
    Changed();
}

void AnimationGraphPanel::RenameAsset(const std::string& name)
{
    if (m_Asset < 0)
        return;
    const auto& library = m_Document.Library;
    const auto previous =
        m_AssetKind == EAnimationAssetKind::Sequence     ? library.Sequences[m_Asset].Name
        : m_AssetKind == EAnimationAssetKind::BlendSpace ? library.BlendSpaces[m_Asset].Name
                                                         : library.Montages[m_Asset].Name;
    if (!m_Document.Library.Rename(m_AssetKind, size_t(m_Asset), name))
    {
        m_Status->SetText("Resource names must be nonempty and unique.");
        return;
    }
    for (auto& source : m_Document.StateSources)
        if (source == previous)
            source = name;
    if (m_Document.OutputSource == previous)
        m_Document.OutputSource = name;
    Changed();
    m_Refresh = false;
}

void AnimationGraphPanel::DeleteAsset()
{
    if (m_Asset < 0)
        return;
    auto& library = m_Document.Library;
    const auto name =
        m_AssetKind == EAnimationAssetKind::Sequence     ? library.Sequences[m_Asset].Name
        : m_AssetKind == EAnimationAssetKind::BlendSpace ? library.BlendSpaces[m_Asset].Name
                                                         : library.Montages[m_Asset].Name;
    bool used = m_Document.OutputSource == name;
    for (const auto& source : m_Document.StateSources)
        used = used || source == name;
    for (const auto& blend : library.BlendSpaces)
        for (const auto& sample : blend.Samples)
            used = used || sample.Sequence == name;
    for (const auto& montage : library.Montages)
        used = used || montage.Sequence == name;
    if (used)
    {
        m_Status->SetText("Disconnect this resource from its consumers before deleting it.");
        return;
    }
    if (m_AssetKind == EAnimationAssetKind::Sequence)
        library.Sequences.erase(library.Sequences.begin() + m_Asset);
    if (m_AssetKind == EAnimationAssetKind::BlendSpace)
        library.BlendSpaces.erase(library.BlendSpaces.begin() + m_Asset);
    if (m_AssetKind == EAnimationAssetKind::Montage)
        library.Montages.erase(library.Montages.begin() + m_Asset);
    m_Asset = -1;
    Changed();
}

void AnimationGraphPanel::PlayMontage(size_t index)
{
    if (!m_Preview)
        Compile();
    const auto montage = m_Document.Library.BuildMontage(index);
    if (m_Preview && montage && m_Preview->GetBaseLayer().GetPlayer().PlayMontage(montage))
    {
        m_Playing = true;
        m_Refresh = true;
        m_Status->SetText("Montage playing over the continuing graph output.");
    }
    else
        m_Status->SetText("Montage rejected. Check its sequence, sections, and fades.");
}

void AnimationGraphPanel::BuildAssetInspector()
{
    auto& library = m_Document.Library;
    // These callbacks capture indices instead of references; undo may replace the library.
    const auto number = [this](const std::string& title, float initial, auto assign)
    {
        Caption(m_Inspector, title);
        Field(
            m_Inspector,
            std::to_string(initial),
            [this, assign](const std::string& text)
            {
                float value;
                if (!ParseFloat(text, value))
                    return;
                assign(value);
                Changed();
                m_Refresh = false;
            }
        );
    };
    Caption(
        m_Inspector,
        "OUTPUT: " + (m_Document.OutputSource.empty() ? std::string("State Machine")
                                                      : m_Document.OutputSource)
    );
    Action(
        m_Inspector,
        "Use state machine as output",
        [this]
        {
            m_Document.OutputSource.clear();
            Changed();
        }
    );
    if (m_Asset < 0)
        return;
    const size_t index = size_t(m_Asset);
    if ((m_AssetKind == EAnimationAssetKind::Sequence && index >= library.Sequences.size()) ||
        (m_AssetKind == EAnimationAssetKind::BlendSpace &&
         index >= library.BlendSpaces.size()) ||
        (m_AssetKind == EAnimationAssetKind::Montage && index >= library.Montages.size()))
    {
        m_Asset = -1;
        return;
    }
    const auto name =
        m_AssetKind == EAnimationAssetKind::Sequence     ? library.Sequences[index].Name
        : m_AssetKind == EAnimationAssetKind::BlendSpace ? library.BlendSpaces[index].Name
                                                         : library.Montages[index].Name;
    Caption(m_Inspector, "RESOURCE NAME");
    Field(
        m_Inspector,
        name,
        [this](const std::string& text)
        {
            RenameAsset(text);
        }
    );
    Action(
        m_Inspector,
        "Delete resource",
        [this]
        {
            DeleteAsset();
        }
    );
    if (m_AssetKind != EAnimationAssetKind::Montage)
    {
        Action(
            m_Inspector,
            "Connect to Output Pose",
            [this, name]
            {
                m_Document.OutputSource = name;
                Changed();
            }
        );
        Action(
            m_Inspector,
            "Preview this source",
            [this, name]
            {
                m_Document.OutputSource = name;
                Changed();
                Compile();
                m_Playing = bool(m_Preview);
            }
        );
        Action(
            m_Inspector,
            m_Document.OutputLooping ? "Output: Loop" : "Output: Once",
            [this]
            {
                m_Document.OutputLooping = !m_Document.OutputLooping;
                Changed();
            }
        );
    }
    if (m_Preview)
    {
        m_PlaybackStatus = CreateRef<GUI::TextBlock>("Runtime preview");
        m_PlaybackStatus->SetFontSize(10);
        m_PlaybackStatus->SetColor(EditorStyle::Get().TextPrimary);
        m_Inspector->AddChild(m_PlaybackStatus).SetFixedSize(28);
        Caption(m_Inspector, "Seek base source (seconds)");
        Field(
            m_Inspector,
            std::to_string(m_Preview->GetBaseLayer().GetPlayer().GetTime()),
            [this](const std::string& text)
            {
                float value;
                if (m_Preview && ParseFloat(text, value))
                    m_Preview->GetBaseLayer().GetPlayer().Seek(value);
            }
        );
    }
    if (m_AssetKind == EAnimationAssetKind::Sequence)
    {
        const auto& clip = library.Sequences[index];
        Caption(m_Inspector, "Skeleton: " + clip.Skeleton->GetName());
        number(
            "Duration (rescales keys and events)",
            clip.Duration,
            [this, index](float value)
            {
                auto& clip = m_Document.Library.Sequences[index];
                if (value <= 0 || clip.Duration <= 0)
                    return;
                const float scale = value / clip.Duration;
                for (auto& track : clip.Tracks)
                {
                    for (auto& key : track.Translations)
                        key.Time *= scale;
                    for (auto& key : track.Rotations)
                        key.Time *= scale;
                    for (auto& key : track.Scales)
                        key.Time *= scale;
                }
                for (auto& marker : clip.Markers)
                    marker.Time *= scale;
                for (auto& notify : clip.Notifies)
                {
                    notify.Time *= scale;
                    notify.Duration *= scale;
                }
                clip.Duration = value;
            }
        );
        Caption(m_Inspector, "SYNCHRONIZATION MARKERS");
        for (size_t i = 0; i < clip.Markers.size(); ++i)
        {
            Field(
                m_Inspector,
                clip.Markers[i].Name,
                [this, index, i](const std::string& value)
                {
                    m_Document.Library.Sequences[index].Markers[i].Name = value;
                    Changed();
                    m_Refresh = false;
                }
            );
            number(
                "Marker time",
                clip.Markers[i].Time,
                [this, index, i](float value)
                {
                    m_Document.Library.Sequences[index].Markers[i].Time = value;
                }
            );
            Action(
                m_Inspector,
                "Remove marker",
                [this, index, i]
                {
                    auto& values = m_Document.Library.Sequences[index].Markers;
                    values.erase(values.begin() + std::ptrdiff_t(i));
                    Changed();
                }
            );
        }
        Action(
            m_Inspector,
            "+ Marker",
            [this, index]
            {
                auto& clip = m_Document.Library.Sequences[index];
                clip.Markers.push_back(
                    {"Marker " + std::to_string(clip.Markers.size() + 1),
                     clip.Duration * 0.25f}
                );
                Changed();
            }
        );
        Caption(m_Inspector, "NOTIFY EVENTS / WINDOWS");
        for (size_t i = 0; i < clip.Notifies.size(); ++i)
        {
            Field(
                m_Inspector,
                clip.Notifies[i].Name,
                [this, index, i](const std::string& value)
                {
                    m_Document.Library.Sequences[index].Notifies[i].Name = value;
                    Changed();
                    m_Refresh = false;
                }
            );
            number(
                "Notify time",
                clip.Notifies[i].Time,
                [this, index, i](float value)
                {
                    m_Document.Library.Sequences[index].Notifies[i].Time = value;
                }
            );
            number(
                "Notify duration (0 = instant)",
                clip.Notifies[i].Duration,
                [this, index, i](float value)
                {
                    m_Document.Library.Sequences[index].Notifies[i].Duration = value;
                }
            );
            Action(
                m_Inspector,
                "Remove notify",
                [this, index, i]
                {
                    auto& values = m_Document.Library.Sequences[index].Notifies;
                    values.erase(values.begin() + std::ptrdiff_t(i));
                    Changed();
                }
            );
        }
        Action(
            m_Inspector,
            "+ Notify",
            [this, index]
            {
                m_Document.Library.Sequences[index].Notifies.push_back(
                    {.Name = "Notify", .Time = 0}
                );
                Changed();
            }
        );
        Caption(m_Inspector, "BONE TRACKS");
        for (size_t i = 0; i < clip.Tracks.size(); ++i)
        {
            Caption(m_Inspector, "Bone " + std::to_string(clip.Tracks[i].BoneIndex));
            const auto keys =
                [&, this](const std::string& channel, auto member, auto interpolation)
            {
                const auto& values = clip.Tracks[i].*member;
                const auto mode = clip.Tracks[i].*interpolation;
                Action(
                    m_Inspector,
                    channel + (mode == EBoneInterpolation::Step ? " interpolation: Step"
                                                                : " interpolation: Linear"),
                    [this, index, i, interpolation]
                    {
                        auto& mode =
                            m_Document.Library.Sequences[index].Tracks[i].*interpolation;
                        mode = mode == EBoneInterpolation::Step ? EBoneInterpolation::Linear
                                                                : EBoneInterpolation::Step;
                        Changed();
                    }
                );
                for (size_t j = 0; j < values.size(); ++j)
                {
                    number(
                        channel + " key time",
                        values[j].Time,
                        [this, index, i, j, member](float value)
                        {
                            (m_Document.Library.Sequences[index].Tracks[i].*member)[j].Time =
                                value;
                        }
                    );
                    for (int axis = 0; axis < values[j].Value.length(); ++axis)
                    {
                        number(
                            channel + " XYZW"[axis + 1],
                            values[j].Value[axis],
                            [this, index, i, j, member, axis](float value)
                            {
                                (m_Document.Library.Sequences[index].Tracks[i].*member)[j]
                                    .Value[axis] = value;
                            }
                        );
                    }
                    Action(
                        m_Inspector,
                        "Remove " + channel + " key",
                        [this, index, i, j, member]
                        {
                            auto& keys = m_Document.Library.Sequences[index].Tracks[i].*member;
                            keys.erase(keys.begin() + std::ptrdiff_t(j));
                            Changed();
                        }
                    );
                }
                Action(
                    m_Inspector,
                    "+ " + channel + " key",
                    [this, index, i, member, channel]
                    {
                        auto& clip = m_Document.Library.Sequences[index];
                        auto& keys = clip.Tracks[i].*member;
                        using Key = typename std::decay_t<decltype(keys)>::value_type;
                        Key key;
                        if constexpr (!std::is_same_v<Key, SRotationAnimationKey>)
                            if (channel == "Scale")
                                key.Value = glm::vec3(1);
                        if (!keys.empty())
                        {
                            key.Value = keys.back().Value;
                            key.Time = clip.Duration;
                            if (keys.back().Time == clip.Duration)
                            {
                                const float previous =
                                    keys.size() > 1 ? keys[keys.size() - 2].Time : 0;
                                key.Time = (previous + clip.Duration) * 0.5f;
                                if (key.Time <= previous || key.Time >= clip.Duration)
                                    return;
                            }
                        }
                        const auto position =
                            std::ranges::lower_bound(keys, key.Time, {}, &Key::Time);
                        keys.insert(position, key);
                        Changed();
                    }
                );
            };
            keys(
                "Translation",
                &SBoneAnimationTrack::Translations,
                &SBoneAnimationTrack::TranslationInterpolation
            );
            keys(
                "Rotation",
                &SBoneAnimationTrack::Rotations,
                &SBoneAnimationTrack::RotationInterpolation
            );
            keys(
                "Scale",
                &SBoneAnimationTrack::Scales,
                &SBoneAnimationTrack::ScaleInterpolation
            );
            Action(
                m_Inspector,
                "Remove track",
                [this, index, i]
                {
                    auto& tracks = m_Document.Library.Sequences[index].Tracks;
                    tracks.erase(tracks.begin() + std::ptrdiff_t(i));
                    Changed();
                }
            );
        }
        Action(
            m_Inspector,
            "+ Bone track (next unanimated bone)",
            [this, index]
            {
                auto& clip = m_Document.Library.Sequences[index];
                for (size_t bone = 0; bone < clip.Skeleton->GetBones().size(); ++bone)
                {
                    bool found = false;
                    for (const auto& track : clip.Tracks)
                        found = found || track.BoneIndex == bone;
                    if (found)
                        continue;
                    SBoneAnimationTrack track;
                    track.BoneIndex = uint32_t(bone);
                    const auto& pose = clip.Skeleton->GetBones()[bone].ReferenceTransform;
                    track.Translations = {
                        {0, pose.Translation},
                        {clip.Duration, pose.Translation}
                    };
                    track.Rotations = {{0, pose.Rotation}, {clip.Duration, pose.Rotation}};
                    track.Scales = {{0, pose.Scale}, {clip.Duration, pose.Scale}};
                    clip.Tracks.push_back(track);
                    Changed();
                    return;
                }
            }
        );
    }
    else if (m_AssetKind == EAnimationAssetKind::BlendSpace)
    {
        const auto& blend = library.BlendSpaces[index];
        Caption(m_Inspector, blend.TwoDimensional ? "BLEND SPACE 2D" : "BLEND SPACE 1D");
        Caption(m_Inspector, "X parameter binding");
        Field(
            m_Inspector,
            blend.XParameter,
            [this, index](const std::string& value)
            {
                m_Document.Library.BlendSpaces[index].XParameter = value;
                Changed();
                m_Refresh = false;
            }
        );
        if (blend.TwoDimensional)
        {
            Caption(m_Inspector, "Y parameter binding");
            Field(
                m_Inspector,
                blend.YParameter,
                [this, index](const std::string& value)
                {
                    m_Document.Library.BlendSpaces[index].YParameter = value;
                    Changed();
                    m_Refresh = false;
                }
            );
        }
        for (size_t i = 0; i < blend.Samples.size(); ++i)
        {
            Caption(m_Inspector, "SAMPLE " + std::to_string(i + 1) + " / sequence name");
            Field(
                m_Inspector,
                blend.Samples[i].Sequence,
                [this, index, i](const std::string& value)
                {
                    m_Document.Library.BlendSpaces[index].Samples[i].Sequence = value;
                    Changed();
                    m_Refresh = false;
                }
            );
            number(
                "X",
                blend.Samples[i].Position.x,
                [this, index, i](float value)
                {
                    m_Document.Library.BlendSpaces[index].Samples[i].Position.x = value;
                }
            );
            if (blend.TwoDimensional)
                number(
                    "Y",
                    blend.Samples[i].Position.y,
                    [this, index, i](float value)
                    {
                        m_Document.Library.BlendSpaces[index].Samples[i].Position.y = value;
                    }
                );
            Action(
                m_Inspector,
                "Remove sample",
                [this, index, i]
                {
                    auto& samples = m_Document.Library.BlendSpaces[index].Samples;
                    samples.erase(samples.begin() + std::ptrdiff_t(i));
                    Changed();
                }
            );
        }
        Action(
            m_Inspector,
            "+ Sample",
            [this, index]
            {
                if (m_Document.Library.Sequences.empty())
                    return;
                auto& blend = m_Document.Library.BlendSpaces[index];
                float x = 0;
                for (const auto& sample : blend.Samples)
                    x = std::max(x, sample.Position.x + 1);
                blend.Samples.push_back({m_Document.Library.Sequences.front().Name, {x, 0}});
                Changed();
            }
        );
    }
    else
    {
        const auto& montage = library.Montages[index];
        Caption(m_Inspector, "MONTAGE / LAYER ACTION");
        Caption(m_Inspector, "Sequence name");
        Field(
            m_Inspector,
            montage.Sequence,
            [this, index](const std::string& value)
            {
                m_Document.Library.Montages[index].Sequence = value;
                Changed();
                m_Refresh = false;
            }
        );
        number(
            "Blend in (seconds)",
            montage.BlendIn,
            [this, index](float value)
            {
                m_Document.Library.Montages[index].BlendIn = value;
            }
        );
        number(
            "Blend out (seconds)",
            montage.BlendOut,
            [this, index](float value)
            {
                m_Document.Library.Montages[index].BlendOut = value;
            }
        );
        Action(
            m_Inspector,
            montage.Interruptible ? "Interruptible: Yes" : "Interruptible: No",
            [this, index]
            {
                auto& value = m_Document.Library.Montages[index].Interruptible;
                value = !value;
                Changed();
            }
        );
        Action(
            m_Inspector,
            "Play montage over graph",
            [this, index]
            {
                PlayMontage(index);
            }
        );
        Action(
            m_Inspector,
            "Stop montage",
            [this]
            {
                if (m_Preview)
                    m_Preview->GetBaseLayer().GetPlayer().StopMontage();
            }
        );
        Caption(m_Inspector, "SECTIONS / PLAYBACK ORDER");
        for (size_t i = 0; i < montage.Sections.size(); ++i)
        {
            const auto& section = montage.Sections[i];
            Field(
                m_Inspector,
                section.Name,
                [this, index, i](const std::string& value)
                {
                    auto& montage = m_Document.Library.Montages[index];
                    const auto previous = montage.Sections[i].Name;
                    montage.Sections[i].Name = value;
                    for (auto& section : montage.Sections)
                        if (section.NextSection == previous)
                            section.NextSection = value;
                    Changed();
                    m_Refresh = false;
                }
            );
            number(
                "Section start (seconds)",
                section.StartTime,
                [this, index, i](float value)
                {
                    m_Document.Library.Montages[index].Sections[i].StartTime = value;
                }
            );
            Caption(m_Inspector, "Next: * = timeline order, empty = end");
            Field(
                m_Inspector,
                section.NextSection.value_or("*"),
                [this, index, i](const std::string& value)
                {
                    auto& next = m_Document.Library.Montages[index].Sections[i].NextSection;
                    if (value == "*")
                        next.reset();
                    else
                        next = value;
                    Changed();
                    m_Refresh = false;
                }
            );
            Action(
                m_Inspector,
                "Jump to " + section.Name,
                [this, name = section.Name]
                {
                    if (m_Preview)
                        m_Preview->GetBaseLayer().GetPlayer().JumpToMontageSection(name);
                }
            );
            Action(
                m_Inspector,
                "Remove section",
                [this, index, i]
                {
                    auto& values = m_Document.Library.Montages[index].Sections;
                    const auto name = values[i].Name;
                    values.erase(values.begin() + std::ptrdiff_t(i));
                    for (auto& section : values)
                        if (section.NextSection == name)
                            section.NextSection.reset();
                    Changed();
                }
            );
        }
        Action(
            m_Inspector,
            "+ Section",
            [this, index]
            {
                auto& montage = m_Document.Library.Montages[index];
                float duration = 0;
                for (const auto& clip : m_Document.Library.Sequences)
                    if (clip.Name == montage.Sequence)
                        duration = clip.Duration;
                const float start =
                    montage.Sections.empty()
                        ? 0
                        : (montage.Sections.back().StartTime + duration) * 0.5f;
                montage.Sections.push_back(
                    {"Section " + std::to_string(montage.Sections.size() + 1), start}
                );
                Changed();
            }
        );
    }
}
