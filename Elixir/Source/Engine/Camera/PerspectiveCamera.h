#pragma once

#include <Engine/Camera/Camera.h>

namespace Elixir
{
    /**
     * @brief 3D perspective camera with position, orientation, and field of view.
     *
     * Projects the scene with depth foreshortening — distant objects appear
     * smaller. Supports full 3D orientation via yaw/pitch angles and provides
     * forward/right/up direction vectors for movement.
     */
    class ELIXIR_API PerspectiveCamera final : public Camera
    {
      public:
        PerspectiveCamera(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);

        void SetProjection(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);
        void SetAspectRatio(float aspectRatio);
        void SetFieldOfView(float fovDegrees);

        float GetYaw() const { return m_Yaw; }
        float GetPitch() const { return m_Pitch; }
        void SetOrientation(float yawDegrees, float pitchDegrees);

        const glm::vec3& GetForwardDirection() const { return m_Front; }
        const glm::vec3& GetRightDirection() const { return m_Right; }
        const glm::vec3& GetUpDirection() const { return m_Up; }

        float GetFieldOfView() const { return m_FOV; }
        float GetAspectRatio() const { return m_AspectRatio; }
        float GetNearPlane() const { return m_NearPlane; }
        float GetFarPlane() const { return m_FarPlane; }

        EProjectionType GetProjectionType() const override
        {
            return EProjectionType::Perspective;
        }

      protected:
        void RecalculateViewMatrix() override;
        void RecalculateProjectionMatrix() override;

      private:
        void UpdateDirectionVectors();

        float m_FOV;
        float m_AspectRatio;
        float m_NearPlane;
        float m_FarPlane;

        float m_Yaw = -90.0f;   // Degrees, -90 = looking along -Z
        float m_Pitch = 0.0f;   // Degrees,   0 = horizontal

        glm::vec3 m_Front = { 0.0f, 0.0f, -1.0f };
        glm::vec3 m_Right = { 1.0f, 0.0f, 0.0f };
        glm::vec3 m_Up    = { 0.0f, 1.0f, 0.0f };

        static constexpr glm::vec3 s_WorldUp = { 0.0f, 1.0f, 0.0f };
    };
}