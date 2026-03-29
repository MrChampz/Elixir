#include "epch.h"
#include "PerspectiveCameraController.h"

#include "Engine/Event/WindowEvent.h"
#include "Engine/Input/InputCodes.h"
#include "Engine/Input/InputManager.h"

namespace Elixir
{
    PerspectiveCameraController::PerspectiveCameraController(
        const float fovDegrees,
        const float aspectRatio
    ) : m_Camera(fovDegrees, aspectRatio, 0.1f, 1000.0f) {}

    void PerspectiveCameraController::Update(const Timestep deltaTime)
    {
        const float dt = deltaTime.GetSeconds();
        const float speed = m_MoveSpeed * dt;

        auto position = m_Camera.GetPosition();
        const auto& front = m_Camera.GetForwardDirection();
        const auto& right = m_Camera.GetRightDirection();

        // Forward/Backward
        if (InputManager::IsKeyPressed(EE_KEY_W))
            position += front * speed;
        if (InputManager::IsKeyPressed(EE_KEY_S))
            position -= front * speed;

        // Strafe left/right
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

    void PerspectiveCameraController::ProcessEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<FramebufferResizeEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::HandleFramebufferResize));
        dispatcher.Dispatch<MouseScrolledEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::HandleMouseScrolled));
        dispatcher.Dispatch<MouseMovedEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::HandleMouseMoved));
        dispatcher.Dispatch<MouseButtonPressedEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::HandleMouseButtonPressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(EE_BIND_EVENT_FN(PerspectiveCameraController::HandleMouseButtonReleased));
    }

    bool PerspectiveCameraController::HandleFramebufferResize(const FramebufferResizeEvent& event)
    {
        if (event.GetHeight() == 0) return false;

        const float aspectRatio = (float)event.GetWidth() / (float)event.GetHeight();
        m_Camera.SetAspectRatio(aspectRatio);

        return false;
    }

    bool PerspectiveCameraController::HandleMouseScrolled(const MouseScrolledEvent& event)
    {
        float fov = m_Camera.GetFieldOfView() - event.GetOffsetY() * 2.0f;
        fov = glm::clamp(fov, 1.0f, 120.0f);
        m_Camera.SetFieldOfView(fov);

        return false;
    }

    bool PerspectiveCameraController::HandleMouseMoved(const MouseMovedEvent& event)
    {
        if (!m_MouseLookActive) return false;

        const auto [mouseX, mouseY] = event.GetPosition();

        if (m_FirstMouse)
        {
            m_LastMousePos = { mouseX, mouseY };
            m_FirstMouse = false;
            return false;
        }

        const float deltaX = mouseX - m_LastMousePos.x;
        const float deltaY = m_LastMousePos.y - mouseY;

        m_LastMousePos = { mouseX, mouseY };

        m_Yaw += deltaX * m_MouseSensitivity;
        m_Pitch += deltaY * m_MouseSensitivity;

        m_Camera.SetOrientation(m_Yaw, m_Pitch);

        return false;
    }

    bool PerspectiveCameraController::HandleMouseButtonPressed(
        const MouseButtonPressedEvent& event
    )
    {
        if (event.GetMouseButton() == EE_MOUSE_BUTTON_RIGHT)
        {
            m_MouseLookActive = true;
            m_FirstMouse = true;
        }

        return false;
    }

    bool PerspectiveCameraController::HandleMouseButtonReleased(
        const MouseButtonReleasedEvent& event
    )
    {
        if (event.GetMouseButton() == EE_MOUSE_BUTTON_RIGHT)
        {
            m_MouseLookActive = false;
        }

        return false;
    }
}