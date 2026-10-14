#pragma once

#include "../EditorPanel.h"
#include <Engine/Aether/Manager.h>
#include <Engine/Camera/ArcBallCameraController.h>
#include <array>
#include "../../VFX/EditorVFXManager.h"

/** Displays a continuous strand scene with keyboard-controlled instance translation. */
class StrandsPanel final : public EditorPanel
{
public:
    /** Creates one persistent instance. The manager and context must outlive the panel. */
    StrandsPanel(EditorVFXManager& manager, GraphicsContext& context, const std::filesystem::path& effect);
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
    /** Shows reference objects for the tentacle and climbing ivy samples. */
    bool ShouldShowReferenceObjects() const { return m_SampleIndex == 0 || m_SampleIndex == 5; }
    /** Shows the wall used by the climbing ivy sample. */
    bool ShouldShowWall() const { return m_SampleIndex == 5; }
    /** Shows the fixed utility poles for the two-ended cable sample. */
    bool ShouldShowPowerPoles() const { return m_SampleIndex == 8; }
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

    EditorVFXManager& m_Manager;
    GraphicsContext& m_Context;
    Ref<Aether::SystemInstance> m_Instance;
    std::filesystem::path m_EffectDirectory;
    std::string m_SampleName, m_LoadError;
    uint32_t m_StrandCount = 0;
    int m_SampleIndex = 0, m_RequestedSample = -1;
    ArcBallCameraController m_Camera{60, 1};
    glm::vec3 m_Position{};
    Ref<GUI::Widget> m_Preview, m_Toolbar, m_DebugLegend;
    Ref<GUI::TextBlock> m_Status, m_DebugLabel, m_GrowthLabel, m_NavigationHint;
    bool m_Debug = false, m_DebugRequested = false;
    bool m_GrowthPaused = false, m_GrowthRequested = false;
    std::array<bool, 4> m_MoveKeys{};
    bool m_Active = false, m_InputBlocked = false, m_ResetRequested = false;
    bool m_OrbitDragging = false, m_PanDragging = false;
};
