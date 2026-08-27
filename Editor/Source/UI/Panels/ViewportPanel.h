#pragma once

#include "../EditorPanel.h"

#include <Engine/GUI/WidgetAnimation.h>

#include <vector>

// The scene viewport: the floating chrome that would normally sit on top of a real render
// (Hierarchy, Inspector, a transform toolbar, and a stats readout). The viewport background
// itself is just a neutral placeholder - the real scene render lands there later.
//
// There's no real scene graph behind any of this - m_Inspector and the tool-mode/play/
// hierarchy-selection state below are plain member variables standing in for a document
// model, just enough for the panels to be genuinely interactive (click, type, toggle) for
// demo purposes.
class ViewportPanel final : public EditorPanel
{
public:
    const char* GetName() const override { return "Viewport"; }

    Ref<GUI::Widget> Build() override;
    void OnUpdate(Timestep frameTime) override;

private:
    // root is the panel's own Canvas - every floating piece below anchors into it directly,
    // the same way EditorUI anchors the menu/tab bars into its own root Canvas.
    void BuildToolbar(const Ref<GUI::Canvas>& root);
    void BuildHierarchyPanel(const Ref<GUI::Canvas>& root);
    void BuildInspectorPanel(const Ref<GUI::Canvas>& root);
    void BuildStatsOverlay(const Ref<GUI::Canvas>& root);

    // Inspector helpers: a section is a collapsible-looking (but not actually collapsible
    // yet) header bar followed by a handful of label/value rows. Passing a non-null
    // enabledValue wires the header's own checkbox to that flag (Mesh Renderer/Rigidbody);
    // passing a value/component reference to the row helpers wires the field itself to be
    // editable, writing straight back into the referenced member on change.
    void AddInspectorSectionHeader(
        const Ref<GUI::VerticalBox>& list,
        const std::string& name,
        bool* enabledValue = nullptr,
        const char* iconPath = nullptr
    );
    void AddInspectorRow(const Ref<GUI::VerticalBox>& list, const std::string& label, std::string& value, bool monospace = false);
    void AddInspectorToggleRow(const Ref<GUI::VerticalBox>& list, const std::string& label, bool& value);
    void AddInspectorVectorRow(const Ref<GUI::VerticalBox>& list, const std::string& label, glm::vec3& value);

    void SetActiveToolMode(int index);
    void SetPlaying(bool playing);
    void SetSelectedHierarchyRow(int index);
    void SetHierarchyPanelOpen(bool open, bool animate = true);
    void SetInspectorPanelOpen(bool open, bool animate = true);
    void AnimatePanel(
        const Ref<GUI::Widget>& panel,
        GUI::WidgetAnimation& animation,
        bool open,
        const glm::vec2& offset
    );
    void AnimatePanelToggle(
        const Ref<GUI::Widget>& toggle,
        GUI::WidgetAnimation& animation,
        bool panelOpen
    );

    // --- Toolbar state ---
    std::vector<Ref<GUI::Widget>> m_ToolModeSwatches; // 0 = Move, 1 = Rotate, 2 = Scale
    std::vector<Ref<GUI::Icon>> m_ToolModeIcons;
    int m_ActiveToolMode = 1;
    Ref<GUI::Widget> m_PlayButton;
    Ref<GUI::Widget> m_PauseButton;
    Ref<GUI::Icon> m_PlayIcon;
    Ref<GUI::Icon> m_PauseIcon;
    bool m_IsPlaying = true;

    // --- Hierarchy state ---
    Ref<GUI::Widget> m_HierarchyPanel;
    Ref<GUI::Widget> m_HierarchyPanelToggle;
    Scope<GUI::WidgetAnimation> m_HierarchyPanelAnimation;
    Scope<GUI::WidgetAnimation> m_HierarchyPanelToggleAnimation;
    struct SHierarchyRow
    {
        Ref<GUI::Widget> Row;
        Ref<GUI::TextBlock> Label;
    };
    std::vector<SHierarchyRow> m_HierarchyRows;
    int m_SelectedHierarchyIndex = 2; // "Player", matching the mock's initial selection

    // --- Inspector state (stand-in for a real selected-entity data model) ---
    Ref<GUI::Widget> m_InspectorPanel;
    Ref<GUI::Widget> m_InspectorPanelToggle;
    Scope<GUI::WidgetAnimation> m_InspectorPanelAnimation;
    Scope<GUI::WidgetAnimation> m_InspectorPanelToggleAnimation;
    struct SInspectorState
    {
        std::string Tag = "Player";
        std::string Layer = "Default";
        glm::vec3 Position{ 0.0f, 1.2f, -3.4f };
        glm::vec3 Rotation{ 0.0f, 180.0f, 0.0f };
        glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };
        bool MeshRendererEnabled = true;
        std::string Mesh = "PlayerCapsule";
        std::string Material = "PlayerMat";
        bool CastShadows = true;
        bool RigidbodyEnabled = true;
        std::string Mass = "1.0";
        bool UseGravity = true;
        std::string MoveSpeed = "6.5";
        std::string JumpHeight = "2.2";
        bool GroundCheck = true;
    } m_Inspector;
};
