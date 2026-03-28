#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    enum class EProjectionType : uint8_t
    {
        Orthographic,
        Perspective
    };

    /**
     * @brief Base camera class providing view and projection matrices.
     *
     * A Camera defines how the world is observed. It computes a view matrix
     * (where the camera is and what it looks at) and a projection matrix
     * (how the 3D world is mapped to 2D screen space). Derived classes
     * implement specific projection types.
     */
    class ELIXIR_API Camera
    {
      public:
        virtual ~Camera() = default;

        [[nodiscard]] const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        [[nodiscard]] const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
        [[nodiscard]] const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

        [[nodiscard]] const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& position);

        [[nodiscard]] virtual EProjectionType GetProjectionType() const = 0;

      protected:
        Camera() = default;

        void RecalculateViewProjectionMatrix();
        virtual void RecalculateViewMatrix() = 0;
        virtual void RecalculateProjectionMatrix() = 0;

        glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };

        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
        glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);
    };

    // =========================================================================
    // Orthographic Camera
    // =========================================================================

    /**
     * @brief 2D orthographic camera with position, rotation, and zoom.
     *
     * Projects the scene using parallel projection — objects maintain their
     * size regardless of distance. Ideal for UI, 2D games, and top-down views.
     */
    class ELIXIR_API OrthographicCamera final : public Camera
    {
      public:
        OrthographicCamera(float left, float right, float bottom, float top);

        void SetProjection(float left, float right, float bottom, float top);

        [[nodiscard]] float GetRotation() const { return m_Rotation; }
        void SetRotation(float degrees);

        [[nodiscard]] float GetZoomLevel() const { return m_ZoomLevel; }
        void SetZoomLevel(float zoom);

        [[nodiscard]] EProjectionType GetProjectionType() const override
        {
            return EProjectionType::Orthographic;
        }

      protected:
        void RecalculateViewMatrix() override;
        void RecalculateProjectionMatrix() override;

      private:
        float m_Left = -1.0f;
        float m_Right = 1.0f;
        float m_Bottom = -1.0f;
        float m_Top = 1.0f;

        float m_Rotation = 0.0f;   // Degrees around the Z axis
        float m_ZoomLevel = 1.0f;
    };

    // =========================================================================
    // Perspective Camera
    // =========================================================================

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

        [[nodiscard]] float GetYaw() const { return m_Yaw; }
        [[nodiscard]] float GetPitch() const { return m_Pitch; }
        void SetOrientation(float yawDegrees, float pitchDegrees);

        [[nodiscard]] const glm::vec3& GetForwardDirection() const { return m_Front; }
        [[nodiscard]] const glm::vec3& GetRightDirection() const { return m_Right; }
        [[nodiscard]] const glm::vec3& GetUpDirection() const { return m_Up; }

        [[nodiscard]] float GetFieldOfView() const { return m_FOV; }
        [[nodiscard]] float GetAspectRatio() const { return m_AspectRatio; }
        [[nodiscard]] float GetNearPlane() const { return m_NearPlane; }
        [[nodiscard]] float GetFarPlane() const { return m_FarPlane; }

        [[nodiscard]] EProjectionType GetProjectionType() const override
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

        float m_Yaw = -90.0f;     // Degrees, -90 = looking along -Z
        float m_Pitch = 0.0f;     // Degrees, 0 = horizontal

        glm::vec3 m_Front = { 0.0f, 0.0f, -1.0f };
        glm::vec3 m_Right = { 1.0f, 0.0f, 0.0f };
        glm::vec3 m_Up    = { 0.0f, 1.0f, 0.0f };

        static constexpr glm::vec3 WorldUp = { 0.0f, 1.0f, 0.0f };
    };
}
