#include "epch.h"
#include "Camera.h"

#include "glm/gtc/matrix_transform.hpp"

namespace Elixir
{
    // =========================================================================
    // Camera (Base)
    // =========================================================================

    void Camera::SetPosition(const glm::vec3& position)
    {
        m_Position = position;
        RecalculateViewMatrix();
    }

    void Camera::RecalculateViewProjectionMatrix()
    {
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }

    // =========================================================================
    // Orthographic Camera
    // =========================================================================

    OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
        : m_Left(left), m_Right(right), m_Bottom(bottom), m_Top(top)
    {
        RecalculateProjectionMatrix();
        RecalculateViewMatrix();
    }

    void OrthographicCamera::SetProjection(float left, float right, float bottom, float top)
    {
        m_Left = left;
        m_Right = right;
        m_Bottom = bottom;
        m_Top = top;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void OrthographicCamera::SetRotation(float degrees)
    {
        m_Rotation = degrees;
        RecalculateViewMatrix();
    }

    void OrthographicCamera::SetZoomLevel(float zoom)
    {
        m_ZoomLevel = glm::max(zoom, 0.01f);
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void OrthographicCamera::RecalculateViewMatrix()
    {
        const auto transform =
            glm::translate(glm::mat4(1.0f), m_Position)
            * glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0.0f, 0.0f, 1.0f));

        // View matrix is the inverse of the camera transform
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
    }

    // =========================================================================
    // Perspective Camera
    // =========================================================================

    PerspectiveCamera::PerspectiveCamera(float fovDegrees, float aspectRatio, float nearPlane, float farPlane)
        : m_FOV(fovDegrees), m_AspectRatio(aspectRatio), m_NearPlane(nearPlane), m_FarPlane(farPlane)
    {
        UpdateDirectionVectors();
        RecalculateProjectionMatrix();
        RecalculateViewMatrix();
    }

    void PerspectiveCamera::SetProjection(float fovDegrees, float aspectRatio, float nearPlane, float farPlane)
    {
        m_FOV = fovDegrees;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void PerspectiveCamera::SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void PerspectiveCamera::SetFieldOfView(float fovDegrees)
    {
        m_FOV = fovDegrees;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void PerspectiveCamera::SetOrientation(float yawDegrees, float pitchDegrees)
    {
        m_Yaw = yawDegrees;

        // Clamp pitch to prevent flipping at the poles
        m_Pitch = glm::clamp(pitchDegrees, -89.0f, 89.0f);

        UpdateDirectionVectors();
        RecalculateViewMatrix();
    }

    void PerspectiveCamera::RecalculateViewMatrix()
    {
        m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
        RecalculateViewProjectionMatrix();
    }

    void PerspectiveCamera::RecalculateProjectionMatrix()
    {
        m_ProjectionMatrix = glm::perspective(
            glm::radians(m_FOV),
            m_AspectRatio,
            m_NearPlane,
            m_FarPlane
        );
    }

    void PerspectiveCamera::UpdateDirectionVectors()
    {
        const float yawRad = glm::radians(m_Yaw);
        const float pitchRad = glm::radians(m_Pitch);

        glm::vec3 front;
        front.x = glm::cos(yawRad) * glm::cos(pitchRad);
        front.y = glm::sin(pitchRad);
        front.z = glm::sin(yawRad) * glm::cos(pitchRad);

        m_Front = glm::normalize(front);
        m_Right = glm::normalize(glm::cross(m_Front, WorldUp));
        m_Up    = glm::normalize(glm::cross(m_Right, m_Front));
    }
}
