#pragma once

#include <Engine/Camera/CameraController.h>
#include <Engine/Camera/OrthographicCamera.h>

namespace Elixir
{
    class MouseScrolledEvent;
    class MouseMovedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;

    class ELIXIR_API OrthographicCameraController final : public CameraController
    {
      public:
        explicit OrthographicCameraController(float aspectRatio, bool enableRotation = false);

        void Update(Timestep deltaTime) override;
        void ProcessEvent(Event& event) override;

        Camera& GetCamera() override { return m_Camera; }
        const Camera& GetCamera() const override { return m_Camera; }
        OrthographicCamera& GetOrthographicCamera() { return m_Camera; }

        void SetMoveSpeed(const float speed) { m_MoveSpeed = speed; }
        void SetRotationSpeed(const float speed) { m_RotationSpeed = speed; }
        void SetZoomSpeed(const float speed) { m_ZoomSpeed = speed; }

      protected:
        bool HandleFramebufferResize(const FramebufferResizeEvent& event) override;

      private:
        bool HandleMouseScrolled(const MouseScrolledEvent& event);

        float m_AspectRatio;
        float m_ZoomLevel = 1.0f;
        bool m_RotationEnabled;

        OrthographicCamera m_Camera;

        float m_MoveSpeed = 500.0f;     // Units per second
        float m_RotationSpeed = 90.0f;  // Degrees per second
        float m_ZoomSpeed = 0.25f;      // Zoom delta per scroll tick
    };
}