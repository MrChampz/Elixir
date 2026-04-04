#pragma once

#include <Engine/Camera/PerspectiveCamera.h>
#include <Engine/Camera/CameraController.h>

namespace Elixir
{
    class MouseScrolledEvent;
    class MouseMovedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;

    /**
     * @brief Controller for 3D arc-ball (orbit) camera movement.
     *
     * Supports:
     *  - Left mouse drag for orbiting (yaw/pitch) around a focal point
     *  - Middle mouse drag for panning the focal point
     *  - Mouse scroll for dollying (adjusting orbit distance)
     *
     * The camera always looks at the target point from a position on
     * a sphere defined by orbit yaw, pitch, and distance.
     */
    class ELIXIR_API ArcBallCameraController final : public CameraController
    {
      public:
        ArcBallCameraController(float fovDegrees, float aspectRatio);

        void ProcessEvent(Event& event) override;

        Camera& GetCamera() override { return m_Camera; }
        const Camera& GetCamera() const override { return m_Camera; }
        PerspectiveCamera& GetPerspectiveCamera() { return m_Camera; }

        void SetTarget(const glm::vec3& target);
        void SetDistance(float distance);

        void SetOrbitSensitivity(const float sensitivity) { m_OrbitSensitivity = sensitivity; }
        void SetPanSensitivity(const float sensitivity) { m_PanSensitivity = sensitivity; }
        void SetZoomSensitivity(const float sensitivity) { m_ZoomSensitivity = sensitivity; }

        const glm::vec3& GetTarget() const { return m_Target; }
        float GetDistance() const { return m_Distance; }

      protected:
        bool HandleFramebufferResize(const FramebufferResizeEvent& event) override;

      private:
        bool HandleMouseScrolled(const MouseScrolledEvent& event);
        bool HandleMouseMoved(const MouseMovedEvent& event);
        bool HandleMouseButtonPressed(const MouseButtonPressedEvent& event);
        bool HandleMouseButtonReleased(const MouseButtonReleasedEvent& event);

        void UpdateCameraFromOrbit();

        PerspectiveCamera m_Camera;

        // Orbit state
        glm::vec3 m_Target = { 0.0f, 0.0f, 0.0f };
        float m_Distance = 10.0f;
        float m_Yaw = -90.0f;      // Degrees, orbit angle in XZ plane
        float m_Pitch = 0.0f;      // Degrees, orbit angle above/below horizontal

        // Sensitivity
        float m_OrbitSensitivity = 0.25f;   // Degrees per pixel
        float m_PanSensitivity = 0.01f;     // Units per pixel (scaled by distance)
        float m_ZoomSensitivity = 1.0f;     // Distance units per scroll tick

        // Distance limits
        float m_MinDistance = 0.5f;
        float m_MaxDistance = 500.0f;

        // Mouse drag state
        bool m_OrbitActive = false;
        bool m_PanActive = false;
        glm::vec2 m_LastMousePos{};
        bool m_FirstMouse = true;
    };
}
