#pragma once

#include "../EditorPanel.h"
#include <Engine/Camera/ArcBallCameraController.h>
#include <Engine/Mesh/StaticMesh.h>

/** Displays one mesh asset with its own orbit camera and floating asset inspector. */
class MeshPanel final : public EditorPanel
{
public:
    /** Creates a viewer for a loaded mesh. The graphics context must outlive the panel. */
    MeshPanel(GraphicsContext& context, Ref<StaticMesh> mesh, std::filesystem::path path);
    /** Returns the document type. */
    const char* GetName() const override { return "Mesh"; }
    /** Builds the viewport and asset information using the shared Editor styles. */
    Ref<GUI::Widget> Build() override;
    /** Updates camera framing and viewport aspect ratio on the UI thread. */
    void OnUpdate(Timestep frameTime) override;
    /** Activates the document and ends any drag when switching tabs. */
    void OnActivated(bool active) override;
    /** Sets the image produced by this document's render target. */
    void SetPreviewImage(const Ref<Image>& image);
    /** Returns the viewport's desired render extent in physical pixels. */
    Extent3D GetPreviewExtent() const;
    /** Returns whether this tab is active. */
    bool IsActive() const { return m_Active; }
    /** Returns the camera used to render the mesh. */
    const Camera& GetCamera() const { return m_Camera.GetCamera(); }
    /** Routes orbit, pan, and zoom gestures within the viewport. */
    void ProcessEvent(Event& event, bool inputBlocked);

private:
    // Centers and fits the mesh bounds in the current viewport.
    void FrameMesh();
    // Ends orbit and pan gestures without changing the camera pose.
    void StopNavigation();

    GraphicsContext& m_Context;
    Ref<StaticMesh> m_Mesh;
    std::filesystem::path m_Path;
    ArcBallCameraController m_Camera;
    Ref<GUI::Widget> m_Preview, m_Inspector, m_Toolbar;
    bool m_Active = false, m_FrameRequested = false;
    bool m_OrbitDragging = false, m_PanDragging = false;
};
