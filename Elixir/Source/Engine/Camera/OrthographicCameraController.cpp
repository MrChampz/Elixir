#include "epch.h"
#include "OrthographicCameraController.h"

#include "Engine/Event/MouseEvent.h"
#include "Engine/Event/WindowEvent.h"
#include "Engine/Input/InputCodes.h"
#include "Engine/Input/InputManager.h"

namespace Elixir
{
    OrthographicCameraController::OrthographicCameraController(
        const float aspectRatio,
        const bool enableRotation
    ) : m_AspectRatio(aspectRatio),
        m_RotationEnabled(enableRotation),
        m_Camera(-aspectRatio * m_ZoomLevel, aspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel) {}

    void OrthographicCameraController::Update(const Timestep deltaTime)
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

    void OrthographicCameraController::ProcessEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<FramebufferResizeEvent>(EE_BIND_EVENT_FN(OrthographicCameraController::HandleFramebufferResize));
        dispatcher.Dispatch<MouseScrolledEvent>(EE_BIND_EVENT_FN(OrthographicCameraController::HandleMouseScrolled));
    }

    bool OrthographicCameraController::HandleFramebufferResize(const FramebufferResizeEvent& event)
    {
        if (event.GetHeight() == 0) return false;

        m_AspectRatio = (float)event.GetWidth() / (float)event.GetHeight();
        m_Camera.SetProjection(
            -m_AspectRatio * m_ZoomLevel,
            m_AspectRatio * m_ZoomLevel,
            -m_ZoomLevel,
            m_ZoomLevel
        );

        EE_CORE_TRACE("OrthographicCameraController resized: [AspectRatio: {}, Extent: {}].", m_AspectRatio, event.GetExtent())

        return false;
    }

    bool OrthographicCameraController::HandleMouseScrolled(const MouseScrolledEvent& event)
    {
        m_ZoomLevel -= event.GetOffsetY() * m_ZoomSpeed;
        m_ZoomLevel = std::max(m_ZoomLevel, 0.1f);

        m_Camera.SetProjection(
            -m_AspectRatio * m_ZoomLevel,
            m_AspectRatio * m_ZoomLevel,
            -m_ZoomLevel,
            m_ZoomLevel
        );

        return false;
    }
}