#pragma once

#include <Engine.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <array>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Rml
{
    class Context;
    class ElementDocument;
}

class RmlUiRenderer;
class RmlUiSystemInterface;
class GameViewRenderer;
class ContentBrowser;

class Editor final : public Elixir::Application, public Rml::EventListener
{
public:
    Editor();
    ~Editor() override;

    void Render(Timestep frameTime) override;
    void OnEvent(Elixir::Event& event) override;
    void ProcessEvent(Rml::Event& event) override;

private:
    enum class EDockSlot : uint8_t
    {
        Center,
        Left,
        Right,
        Top,
        Bottom,
        Floating
    };

    enum class ERmlInputType
    {
        MouseMove,
        MouseButtonDown,
        MouseButtonUp,
        MouseWheel,
        KeyDown,
        KeyUp,
        TextInput
    };

    struct SRmlInputEvent
    {
        ERmlInputType Type;
        int Value0 = 0;
        int Value1 = 0;
        float WheelDelta = 0.0f;
        Rml::Input::KeyIdentifier Key = Rml::Input::KI_UNKNOWN;
        int Modifiers = 0;
    };

    void QueueInput(SRmlInputEvent event);
    void ProcessPendingInput();
    void UpdateModifierKey(int keyCode, bool pressed);
    int GetModifierState() const;
    void RegisterUiListeners();
    void ProcessDockEvent(Rml::Event& event);
    void DockPanel(const Rml::String& panelId, EDockSlot slot);
    void UndockPanel(const Rml::String& panelId);
    void ActivateDockTab(const Rml::String& panelId);
    void UpdateDockLayout();
    void ResizeDockRegion(EDockSlot slot, float mouseX, float mouseY);
    void SetFloatingPanelPosition(const Rml::String& panelId, float mouseX, float mouseY);
    EDockSlot GetPanelDockSlot(const Rml::String& panelId) const;
    Rml::String GetFirstDockedPanel(EDockSlot slot, const Rml::String& excluded = {}) const;
    bool IsDockSlotOccupied(EDockSlot slot) const;
    void SetWorkspaceTabActive(bool materialEditorActive);
    void SetDockOverlayVisible(bool visible);
    void SetDockPreview(EDockSlot slot, bool visible);
    void OpenMaterialEditor();
    void OpenEffectMaterial(const std::filesystem::path& effectPath);
    void CloseMaterialEditor();
    void SetWindowMenuOpen(bool open);
    void ProcessMaterialNodeEvent(Rml::Event& event);
    void SelectMaterialNode(const Rml::String& nodeId);
    void HandleMaterialPortClick(Rml::Event& event, Rml::Element* port);
    void InitializeMaterialConnectionSegments();
    void UpdateMaterialConnections();
    void ShowMaterialNode(const Rml::String& nodeId);
    void DeleteSelectedMaterialNode();

    Scope<GameViewRenderer> m_GameViewRenderer;
    Scope<ContentBrowser> m_ContentBrowser;
    Scope<RmlUiSystemInterface> m_RmlSystemInterface;
    Scope<RmlUiRenderer> m_RmlRenderer;
    Rml::TextureHandle m_GameViewTexture = 0;
    Rml::Context* m_RmlContext = nullptr;
    Rml::ElementDocument* m_RmlDocument = nullptr;

    std::mutex m_RmlInputMutex;
    std::vector<SRmlInputEvent> m_PendingRmlInput;
    std::array<bool, 8> m_ModifierKeys {};
    std::unordered_map<std::string, EDockSlot> m_DockSlots;
    std::array<Rml::String, 5> m_ActiveDockPanels { "scene-view", "", "", "", "" };
    Rml::String m_DraggedDockPanel;
    EDockSlot m_HoveredDockSlot = EDockSlot::Floating;
    glm::vec2 m_DockDragOffset {};
    float m_LeftDockWidth = 300.0f;
    float m_RightDockWidth = 320.0f;
    float m_TopDockHeight = 220.0f;
    float m_BottomDockHeight = 250.0f;
    glm::vec2 m_LastDockViewportSize {};
    bool m_DockDropHandled = false;
    std::array<bool, 3> m_MaterialConnections { true, false, true };
    Rml::String m_SelectedMaterialNode = "material-node-texture";
    Rml::String m_PendingMaterialOutputPort;
    float m_MaterialNodeDragOffsetX = 0.0f;
    float m_MaterialNodeDragOffsetY = 0.0f;
    bool m_Playing = false;
};
