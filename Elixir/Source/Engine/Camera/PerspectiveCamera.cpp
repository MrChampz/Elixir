#include "epch.h"
#include "PerspectiveCamera.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

namespace Elixir
{
    PerspectiveCamera::PerspectiveCamera(
        const float fovDegrees,
        const float aspectRatio,
        const float nearPlane,
        const float farPlane
    ) : m_FOV(fovDegrees),
        m_AspectRatio(aspectRatio),
        m_NearPlane(nearPlane),
        m_FarPlane(farPlane)
    {
        UpdateDirectionVectors();
        PerspectiveCamera::RecalculateProjectionMatrix();
        PerspectiveCamera::RecalculateViewMatrix();
    }

    void PerspectiveCamera::SetProjection(
        const float fovDegrees,
        const float aspectRatio,
        const float nearPlane,
        const float farPlane
    )
    {
        m_FOV = fovDegrees;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void PerspectiveCamera::SetAspectRatio(const float aspectRatio)
    {
        m_AspectRatio = aspectRatio;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void PerspectiveCamera::SetFieldOfView(const float fovDegrees)
    {
        m_FOV = fovDegrees;
        RecalculateProjectionMatrix();
        RecalculateViewProjectionMatrix();
    }

    void PerspectiveCamera::SetOrientation(const float yawDegrees, const float pitchDegrees)
    {
        m_Yaw = yawDegrees;

        // Clamp pitch to prevent flipping
        m_Pitch = std::clamp(pitchDegrees, -89.0f, 89.0f);

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
        m_ProjectionMatrix[1][1] *= -1.0f;
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
        m_Right = glm::normalize(glm::cross(m_Front, s_WorldUp));
        m_Up    = glm::normalize(glm::cross(m_Right, m_Front));
     }
}