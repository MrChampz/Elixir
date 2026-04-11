#include "epch.h"
#include "OrthographicCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Elixir
{
    OrthographicCamera::OrthographicCamera(
        const float left,
        const float right,
        const float bottom,
        const float top
    ) : m_Left(left), m_Right(right), m_Bottom(bottom), m_Top(top)
    {
        OrthographicCamera::RecalculateProjectionMatrix();
        OrthographicCamera::RecalculateViewMatrix();
    }

    void OrthographicCamera::SetProjection(
        const float left,
        const float right,
        const float bottom,
        const float top
    )
    {
        m_Left = left;
        m_Right = right;
        m_Bottom = bottom;
        m_Top = top;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void OrthographicCamera::SetRotation(const float degrees)
    {
        m_Rotation = degrees;
        RecalculateViewMatrix();
    }

    void OrthographicCamera::SetZoomLevel(const float zoom)
    {
        m_ZoomLevel = glm::max(zoom, 0.01f);
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void OrthographicCamera::RecalculateViewMatrix()
    {
        constexpr auto zAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        auto transform = glm::translate(glm::mat4(1.0f), m_Position);
        transform = glm::rotate(transform, glm::radians(m_Rotation), zAxis);

        // View matrix is the inverse of the camera's transform.
        m_ViewMatrix = glm::inverse(transform);

        RecalculateViewProjectionMatrix();
    }

    void OrthographicCamera::RecalculateProjectionMatrix()
    {
        m_ProjectionMatrix = glm::ortho(
            m_Left * m_ZoomLevel,
            m_Right * m_ZoomLevel,
            m_Bottom * m_ZoomLevel,
            m_Top * m_ZoomLevel
        );
        m_ProjectionMatrix[1][1] *= -1.0f;
    }
}
