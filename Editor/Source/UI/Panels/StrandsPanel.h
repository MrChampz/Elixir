#pragma once

#include "../EditorPanel.h"
#include <Engine/Aether/Manager.h>
#include <Engine/Camera/ArcBallCameraController.h>
#include <array>

/** Displays a continuous strand scene with keyboard-controlled instance translation. */
class StrandsPanel final : public EditorPanel
{
public:
    /** Creates one persistent instance. The manager and context must outlive the panel. */
    StrandsPanel(Aether::Manager& manager, GraphicsContext& context, const std::filesystem::path& effect);
    /** Returns the panel name. */
    const char* GetName() const override { return "Strands"; }
    /** Builds the scene viewport and control hints. */
    Ref<GUI::Widget> Build() override;
    /** Moves the instance and updates the viewport camera. */
    void OnUpdate(Timestep frameTime) override;
    /** Stops navigation when leaving the tab. */
    void OnActivated(bool active) override;
    /** Assigns the scene's display image. */
    void SetPreviewImage(const Ref<Image>& image);
    /** Returns the desired viewport size in physical pixels. */
    Extent3D GetPreviewExtent() const;
    /** Returns whether the tab is selected. */
    bool IsActive() const { return m_Active; }
    /** Shows the particlez reference objects only for the original tentacle sample. */
    bool ShouldShowReferenceObjects() const { return m_SampleIndex == 0; }
    /** Returns the scene camera. */
    const Camera& GetCamera() const { return m_Camera.GetCamera(); }
    /** Routes movement keys and viewport camera gestures. */
    void ProcessEvent(Event& event, bool inputBlocked);
    /** Blocks movement while other editor controls capture input. */
    void SetInputBlocked(bool blocked);

private:
    // Activates a sample while preserving the previous instance if loading fails.
    bool LoadSample(int index);
    // Restores the reference camera and instance origin.
    void ResetPosition();
    // Releases movement keys and camera gestures.
    void StopNavigation();

    Aether::Manager& m_Manager;
    GraphicsContext& m_Context;
    Ref<Aether::SystemInstance> m_Instance;
    std::filesystem::path m_EffectDirectory;
    std::string m_SampleName, m_LoadError;
    uint32_t m_StrandCount = 0;
    int m_SampleIndex = 0, m_RequestedSample = -1;
    ArcBallCameraController m_Camera{60, 1};
    glm::vec3 m_Position{};
    Ref<GUI::Widget> m_Preview, m_Toolbar;
    Ref<GUI::TextBlock> m_Status;
    std::array<bool, 4> m_MoveKeys{};
    bool m_Active = false, m_InputBlocked = false, m_ResetRequested = false;
    bool m_OrbitDragging = false, m_PanDragging = false;
};
