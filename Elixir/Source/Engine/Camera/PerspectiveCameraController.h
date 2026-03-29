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
     * @brief Controller for 3D perspective (first-person) camera movement.
     *
     * Supports:
     *  - WASD for forward/backward/strafe movement
     *  - Space/Shift for vertical movement
     *  - Mouse movement for look-around (yaw/pitch)
     *  - Mouse scroll for FOV zoom
     *
     * Mouse look is activated by holding the right mouse button.
     */
    class ELIXIR_API PerspectiveCameraController final : public CameraController
    {
      public:
        PerspectiveCameraController(float fovDegrees, float aspectRatio);

        void Update(Timestep deltaTime) override;
        void ProcessEvent(Event& event) override;

        Camera& GetCamera() override { return m_Camera; }
        const Camera& GetCamera() const override { return m_Camera; }
        PerspectiveCamera& GetPerspectiveCamera() { return m_Camera; }

        void SetMoveSpeed(const float speed) { m_MoveSpeed = speed; }
        void SetMouseSensitivity(const float sensitivity) { m_MouseSensitivity = sensitivity; }

      protected:
        bool HandleFramebufferResize(const FramebufferResizeEvent& event) override;

      private:
        bool HandleMouseScrolled(const MouseScrolledEvent& event);
        bool HandleMouseMoved(const MouseMovedEvent& event);
        bool HandleMouseButtonPressed(const MouseButtonPressedEvent& event);
        bool HandleMouseButtonReleased(const MouseButtonReleasedEvent& event);

        PerspectiveCamera m_Camera;

        float m_MoveSpeed = 5.0f;          // Units per second
        float m_MouseSensitivity = 0.1f;   // Degrees per pixel

        bool m_MouseLookActive = false;
        glm::vec2 m_LastMousePos{};
        bool m_FirstMouse = true;

        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;
    };
}