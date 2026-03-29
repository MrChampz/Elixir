#pragma once

namespace Elixir
{
    enum class EProjectionType : uint8_t
    {
        Perspective,
        Orthographic
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

        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
        const glm::mat4 GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

        const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& position)
        {
            m_Position = position;
            RecalculateViewMatrix();
        }

        virtual EProjectionType GetProjectionType() const = 0;

      protected:
        Camera() = default;

        void RecalculateViewProjectionMatrix()
        {
            m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
        }

        virtual void RecalculateViewMatrix() = 0;
        virtual void RecalculateProjectionMatrix() = 0;

        glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };

        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
        glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);
    };
}