#pragma once

#include <Engine/Camera/Camera.h>
#include <Engine/Core/Timer.h>
#include <Engine/Event/Event.h>
#include <Engine/Event/MouseEvent.h>
#include <Engine/Event/WindowEvent.h>

namespace Elixir
{
    /**
     * @brief Abstract camera controller that bridges input events to camera transforms.
     *
     * Processes keyboard and mouse input to drive camera movement and rotation.
     * Derived classes implement behavior specific to orthographic or perspective cameras.
     */
    class ELIXIR_API CameraController
    {
      public:
        virtual ~CameraController() = default;

        virtual void OnUpdate(Timestep deltaTime) = 0;
        virtual void OnEvent(Event& event) = 0;
        virtual void OnResize(uint32_t width, uint32_t height) = 0;

        [[nodiscard]] virtual Camera& GetCamera() = 0;
        [[nodiscard]] virtual const Camera& GetCamera() const = 0;

      protected:
        CameraController() = default;
    };

    // =========================================================================
    // Orthographic Camera Controller
    // =========================================================================

    /**
     * @brief Controller for 2D orthographic camera movement.
     *
     * Supports:
     *  - WASD / Arrow keys for panning
     *  - Q/E for rotation
     *  - Mouse scroll wheel for zoom
     *
     * Movement speed scales with the current zoom level so panning
     * feels consistent regardless of how far the camera is zoomed in or out.
     */
    class ELIXIR_API OrthographicCameraController final : public CameraController
    {
      public:
        explicit OrthographicCameraController(float aspectRatio, bool enableRotation = false);

        void OnUpdate(Timestep deltaTime) override;
        void OnEvent(Event& event) override;
        void OnResize(uint32_t width, uint32_t height) override;

        [[nodiscard]] Camera& GetCamera() override { return m_Camera; }
        [[nodiscard]] const Camera& GetCamera() const override { return m_Camera; }
        [[nodiscard]] OrthographicCamera& GetOrthographicCamera() { return m_Camera; }

        void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
        void SetRotationSpeed(float speed) { m_RotationSpeed = speed; }
        void SetZoomSpeed(float speed) { m_ZoomSpeed = speed; }

      private:
        bool OnMouseScrolled(MouseScrolledEvent& event);
        bool OnWindowResize(WindowResizeEvent& event);

        float m_AspectRatio;
        float m_ZoomLevel = 1.0f;
        bool m_RotationEnabled;

        OrthographicCamera m_Camera;

        float m_MoveSpeed = 500.0f;      // Units per second
        float m_RotationSpeed = 90.0f;    // Degrees per second
        float m_ZoomSpeed = 0.25f;        // Zoom delta per scroll tick
    };

    // =========================================================================
    // Perspective Camera Controller
    // =========================================================================

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

        void OnUpdate(Timestep deltaTime) override;
        void OnEvent(Event& event) override;
        void OnResize(uint32_t width, uint32_t height) override;

        [[nodiscard]] Camera& GetCamera() override { return m_Camera; }
        [[nodiscard]] const Camera& GetCamera() const override { return m_Camera; }
        [[nodiscard]] PerspectiveCamera& GetPerspectiveCamera() { return m_Camera; }

        void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
        void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }

      private:
        bool OnMouseScrolled(MouseScrolledEvent& event);
        bool OnMouseMoved(MouseMovedEvent& event);
        bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
        bool OnMouseButtonReleased(MouseButtonReleasedEvent& event);
        bool OnWindowResize(WindowResizeEvent& event);

        PerspectiveCamera m_Camera;

        float m_MoveSpeed = 5.0f;           // Units per second
        float m_MouseSensitivity = 0.1f;     // Degrees per pixel

        bool m_MouseLookActive = false;
        float m_LastMouseX = 0.0f;
        float m_LastMouseY = 0.0f;
        bool m_FirstMouse = true;

        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;
    };
}
