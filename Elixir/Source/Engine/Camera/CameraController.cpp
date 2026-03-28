#include "epch.h"
#include "CameraController.h"

#include <Engine/Input/InputManager.h>
#include <Engine/Input/InputCodes.h>

namespace Elixir
{
    // =========================================================================
    // Orthographic Camera Controller
    // =========================================================================

    OrthographicCameraController::OrthographicCameraController(float aspectRatio, bool enableRotation)
        : m_AspectRatio(aspectRatio)
        , m_RotationEnabled(enableRotation)
        , m_Camera(-aspectRatio * m_ZoomLevel, aspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel)
    {
    }

    void OrthographicCameraController::OnUpdate(Timestep deltaTime)
    {
        const float dt = deltaTime.GetSeconds();
        const float speed = m_MoveSpeed * m_ZoomLevel * dt;

        auto position = m_Camera.GetPosition();

        if (InputManager::IsKeyPressed(EE_KEY_W) || InputManager::IsKeyPressed(EE_KEY_UP))
            position.y += speed;
        if (InputManager::IsKeyPressed(EE_KEY_S) || InputManager::IsKeyPressed(EE_KEY_DOWN))
            position.y -= speed;
        if (InputManager::IsKeyPressed(EE_KEY_A) || InputManager::IsKeyPressed(EE_KEY_LEFT))
            position.x -= speed;
        if (InputManager::IsKeyPressed(EE_KEY_D) || InputManager::IsKeyPressed(EE_KEY_RIGHT))
            position.x += speed;

        m_Camera.SetPosition(position);

        if (m_RotationEnabled)
        {
            float rotation = m_Camera.GetRotation();

            if (InputManager::IsKeyPressed(EE_KEY_Q))
                rotation += m_RotationSpeed * dt;
            if (InputManager::IsKeyPressed(EE_KEY_E))
                rotation -= m_RotationSpeed * dt;

            m_Camera.SetRotation(rotation);
        }
    }

    void OrthographicCameraController::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseScrolledEvent>(EE_BIND_EVENT_FN(OrthographicCameraController::OnMouseScrolled));
        dispatcher.Dispatch<WindowResizeEvent>(EE_BIND_EVENT_FN(OrthographicCameraController::OnWindowResize));
    }

    void OrthographicCameraController::OnResize(uint32_t width, uint32_t height)
    {
        if (height == 0) return;

        m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        m_Camera.SetProjection(
            -m_AspectRatio * m_ZoomLevel,
             m_AspectRatio * m_ZoomLevel,
            -m_ZoomLevel,
             m_ZoomLevel
        );
    }

    bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& event)
    {
        m_ZoomLevel -= event.GetOffsetY() * m_ZoomSpeed;
        m_ZoomLevel = glm::max(m_ZoomLevel, 0.1f);

        m_Camera.SetProjection(
            -m_AspectRatio * m_ZoomLevel,
             m_AspectRatio * m_ZoomLevel,
            -m_ZoomLevel,
             m_ZoomLevel
        );

        return false;
    }

    bool OrthographicCameraController::OnWindowResize(WindowResizeEvent& event)
    {
        OnResize(event.GetWidth(), event.GetHeight());
        return false;
    }

    // =========================================================================
    // Perspective Camera Controller
    // =========================================================================

    PerspectiveCameraController::PerspectiveCameraController(float fovDegrees, float aspectRatio)
        : m_Camera(fovDegrees, aspectRatio, 0.1f, 1000.0f)
    {
    }

    void PerspectiveCameraController::OnUpdate(Timestep deltaTime)
    {
        const float dt = deltaTime.GetSeconds();
        const float speed = m_MoveSpeed * dt;

        auto position = m_Camera.GetPosition();
        const auto& front = m_Camera.GetForwardDirection();
        const auto& right = m_Camera.GetRightDirection();

        // Forward / Backward
        if (InputManager::IsKeyPressed(EE_KEY_W))
            position += front * speed;
        if (InputManager::IsKeyPressed(EE_KEY_S))
            position -= front * speed;

        // Strafe Left / Right
        if (InputManager::IsKeyPressed(EE_KEY_A))
            position -= right * speed;
        if (InputManager::IsKeyPressed(EE_KEY_D))
            position += right * speed;

        // Vertical
        if (InputManager::IsKeyPressed(EE_KEY_SPACE))
            position.y += speed;
        if (InputManager::IsKeyPressed(EE_KEY_LEFT_SHIFT))
            position.y -= speed;

        m_Camera.SetPosition(position);
    }

    void PerspectiveCameraController::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseScrolledEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::OnMouseScrolled));
        dispatcher.Dispatch<MouseMovedEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::OnMouseMoved));
        dispatcher.Dispatch<MouseButtonPressedEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::OnMouseButtonPressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::OnMouseButtonReleased));
        dispatcher.Dispatch<WindowResizeEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::OnWindowResize));
    }

    void PerspectiveCameraController::OnResize(uint32_t width, uint32_t height)
    {
        if (height == 0) return;

        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        m_Camera.SetAspectRatio(aspectRatio);
    }

    bool PerspectiveCameraController::OnMouseScrolled(MouseScrolledEvent& event)
    {
        float fov = m_Camera.GetFieldOfView() - event.GetOffsetY() * 2.0f;
        fov = glm::clamp(fov, 1.0f, 120.0f);
        m_Camera.SetFieldOfView(fov);
        return false;
    }

    bool PerspectiveCameraController::OnMouseMoved(MouseMovedEvent& event)
    {
        if (!m_MouseLookActive) return false;

        const float mouseX = event.GetX();
        const float mouseY = event.GetY();

        if (m_FirstMouse)
        {
            m_LastMouseX = mouseX;
            m_LastMouseY = mouseY;
            m_FirstMouse = false;
            return false;
        }

        const float deltaX = mouseX - m_LastMouseX;
        const float deltaY = m_LastMouseY - mouseY; // Reversed: y goes bottom-to-top

        m_LastMouseX = mouseX;
        m_LastMouseY = mouseY;

        m_Yaw   += deltaX * m_MouseSensitivity;
        m_Pitch += deltaY * m_MouseSensitivity;

        m_Camera.SetOrientation(m_Yaw, m_Pitch);

        return false;
    }

    bool PerspectiveCameraController::OnMouseButtonPressed(MouseButtonPressedEvent& event)
    {
        if (event.GetMouseButton() == EE_MOUSE_BUTTON_RIGHT)
        {
            m_MouseLookActive = true;
            m_FirstMouse = true;
        }

        return false;
    }

    bool PerspectiveCameraController::OnMouseButtonReleased(MouseButtonReleasedEvent& event)
    {
        if (event.GetMouseButton() == EE_MOUSE_BUTTON_RIGHT)
        {
            m_MouseLookActive = false;
        }

        return false;
    }

    bool PerspectiveCameraController::OnWindowResize(WindowResizeEvent& event)
    {
        OnResize(event.GetWidth(), event.GetHeight());
        return false;
    }
}
