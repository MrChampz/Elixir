#include "epch.h"
#include "ArcBallCameraController.h"

#include "Engine/Event/WindowEvent.h"
#include "Engine/Event/KeyEvent.h"
#include "Engine/Event/MouseEvent.h"
#include "Engine/Input/InputCodes.h"

namespace Elixir
{
    ArcBallCameraController::ArcBallCameraController(
        const float fovDegrees,
        const float aspectRatio
    ) : m_Camera(fovDegrees, aspectRatio, 0.1f, 1000.0f)
    {
        UpdateCameraFromOrbit();
    }

    void ArcBallCameraController::ProcessEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<FramebufferResizeEvent>(EE_BIND_EVENT_FN(ArcBallCameraController::HandleFramebufferResize));
        dispatcher.Dispatch<KeyPressedEvent>(EE_BIND_EVENT_FN(ArcBallCameraController::HandleKeyPressed));
        dispatcher.Dispatch<KeyReleasedEvent>(EE_BIND_EVENT_FN(ArcBallCameraController::HandleKeyReleased));
        dispatcher.Dispatch<MouseScrolledEvent>(EE_BIND_EVENT_FN(ArcBallCameraController::HandleMouseScrolled));
        dispatcher.Dispatch<MouseMovedEvent>(EE_BIND_EVENT_FN(ArcBallCameraController::HandleMouseMoved));
        dispatcher.Dispatch<MouseButtonPressedEvent>(EE_BIND_EVENT_FN(ArcBallCameraController::HandleMouseButtonPressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(EE_BIND_EVENT_FN(ArcBallCameraController::HandleMouseButtonReleased));
    }

    bool ArcBallCameraController::HandleFramebufferResize(const FramebufferResizeEvent& event)
    {
        if (event.GetHeight() == 0) return false;

        const float aspectRatio = (float)event.GetWidth() / (float)event.GetHeight();
        m_Camera.SetAspectRatio(aspectRatio);

        EE_CORE_TRACE("ArcBallCameraController resized: [AspectRatio: {}, Extent: {}].", aspectRatio, event.GetExtent())

        return false;
    }

    bool ArcBallCameraController::HandleKeyPressed(const KeyPressedEvent& event)
    {
        // Every key press reports the live modifier state, so this also resyncs
        // Alt when a release was missed (the window losing focus mid-chord).
        m_AltActive = event.IsAltPressed()
            || event.GetKeyCode() == EE_KEY_LEFT_ALT
            || event.GetKeyCode() == EE_KEY_RIGHT_ALT;

        return false;
    }

    bool ArcBallCameraController::HandleKeyReleased(const KeyReleasedEvent& event)
    {
        if (event.GetKeyCode() == EE_KEY_LEFT_ALT || event.GetKeyCode() == EE_KEY_RIGHT_ALT)
            m_AltActive = false;

        return false;
    }

    void ArcBallCameraController::SetTarget(const glm::vec3& target)
    {
        m_Target = target;
        UpdateCameraFromOrbit();
    }

    void ArcBallCameraController::SetDistance(const float distance)
    {
        m_Distance = glm::clamp(distance, m_MinDistance, m_MaxDistance);
        UpdateCameraFromOrbit();
    }

    bool ArcBallCameraController::HandleMouseScrolled(const MouseScrolledEvent& event)
    {
        m_Distance -= event.GetOffsetY() * m_ZoomSensitivity;
        m_Distance = glm::clamp(m_Distance, m_MinDistance, m_MaxDistance);

        UpdateCameraFromOrbit();

        return false;
    }

    bool ArcBallCameraController::HandleMouseButtonPressed(const MouseButtonPressedEvent& event)
    {
        // Alt arms the drag (Unreal/Maya convention), leaving a bare click free for
        // the viewport. Only the press is gated, so releasing Alt mid-drag -- or the
        // key state going stale -- can't strand an orbit that is already running.
        if (!m_AltActive) return false;

        if (event.GetMouseButton() == EE_MOUSE_BUTTON_LEFT)
        {
            m_OrbitActive = true;
            m_FirstMouse = true;
        }
        else if (event.GetMouseButton() == EE_MOUSE_BUTTON_MIDDLE)
        {
            m_PanActive = true;
            m_FirstMouse = true;
        }

        return false;
    }

    bool ArcBallCameraController::HandleMouseButtonReleased(const MouseButtonReleasedEvent& event)
    {
        if (event.GetMouseButton() == EE_MOUSE_BUTTON_LEFT)
        {
            m_OrbitActive = false;
        }
        else if (event.GetMouseButton() == EE_MOUSE_BUTTON_MIDDLE)
        {
            m_PanActive = false;
        }

        return false;
    }

    bool ArcBallCameraController::HandleMouseMoved(const MouseMovedEvent& event)
    {
        if (!m_OrbitActive && !m_PanActive) return false;

        const auto [mouseX, mouseY] = event.GetPosition();

        if (m_FirstMouse)
        {
            m_LastMousePos = { mouseX, mouseY };
            m_FirstMouse = false;
            return false;
        }

        const float deltaX = mouseX - m_LastMousePos.x;
        const float deltaY = mouseY - m_LastMousePos.y;

        m_LastMousePos = { mouseX, mouseY };

        if (m_OrbitActive)
        {
            m_Yaw += deltaX * m_OrbitSensitivity;
            m_Pitch -= deltaY * m_OrbitSensitivity;
            m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
        }

        if (m_PanActive)
        {
            // Pan in the camera's local right/up plane.
            // Scale by distance so panning feels consistent at any zoom level.
            const glm::vec3& right = m_Camera.GetRightDirection();
            const glm::vec3& up = m_Camera.GetUpDirection();

            const float panScale = m_PanSensitivity * m_Distance;

            m_Target -= right * (deltaX * panScale);
            m_Target += up * (deltaY * panScale);
        }

        UpdateCameraFromOrbit();

        return false;
    }

    void ArcBallCameraController::UpdateCameraFromOrbit()
    {
        // Camera position on a sphere around the target.
        // Orbit yaw/pitch define the direction FROM target TO camera.
        const float yawRad = glm::radians(m_Yaw);
        const float pitchRad = glm::radians(m_Pitch);

        glm::vec3 orbitDirection;
        orbitDirection.x = glm::cos(pitchRad) * glm::cos(yawRad);
        orbitDirection.y = glm::sin(pitchRad);
        orbitDirection.z = glm::cos(pitchRad) * glm::sin(yawRad);

        const glm::vec3 cameraPosition = m_Target + glm::normalize(orbitDirection) * m_Distance;

        // Look direction is FROM camera TO target (reverse of orbit direction).
        // Convert to yaw/pitch for PerspectiveCamera::SetOrientation.
        const glm::vec3 lookDirection = glm::normalize(m_Target - cameraPosition);
        const float clampedYAxis = glm::clamp(lookDirection.y, -1.0f, 1.0f);

        const float lookYaw = glm::degrees(glm::atan(lookDirection.z, lookDirection.x));
        const float lookPitch = glm::degrees(glm::asin(clampedYAxis));

        m_Camera.SetOrientation(lookYaw, lookPitch);
        m_Camera.SetPosition(cameraPosition);
    }
}
