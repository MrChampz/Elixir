#pragma once

#include <Engine/Camera/Camera.h>

namespace Elixir
{
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

        float GetRotation() const { return m_Rotation; }
        void SetRotation(float degrees);

        float GetZoomLevel() const { return m_ZoomLevel; }
        void SetZoomLevel(float zoom);

        EProjectionType GetProjectionType() const override
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

        float m_Rotation = 0.0f; // Degrees around z-axis
        float m_ZoomLevel = 1.0f;
    };
}