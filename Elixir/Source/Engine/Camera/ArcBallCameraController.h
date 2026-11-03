#pragma once

#include <Engine/Camera/PerspectiveCamera.h>
#include <Engine/Camera/CameraController.h>

namespace Elixir
{
    class MouseScrolledEvent;
    class MouseMovedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;

    /**
     * @brief Controller for 3D arc-ball (orbit) camera movement.
     *
     * Supports:
     *  - Left mouse drag for orbiting (yaw/pitch) around a focal point
     *  - Middle mouse drag for panning the focal point
     *  - Mouse scroll for dollying (adjusting orbit distance)
     *
     * The camera always looks at the target point from a position on
     * a sphere defined by orbit yaw, pitch, and distance.
     */
    class ELIXIR_API ArcBallCameraController final : public CameraController
    {
      public:
        /**
         * @brief Create an orbit camera controller.
         * @param fovDegrees Vertical field of view in degrees.
         * @param aspectRatio Viewport width divided by height.
         */
        ArcBallCameraController(float fovDegrees, float aspectRatio);

        /**
         * @brief Process an input or framebuffer resize event.
         * @param event Event to dispatch to the controller handlers.
         */
        void ProcessEvent(Event& event) override;

        /** @brief Get the controlled camera. */
        Camera& GetCamera() override { return m_Camera; }

        /** @brief Get the controlled camera. */
        const Camera& GetCamera() const override { return m_Camera; }

        /** @brief Get the controlled perspective camera. */
        PerspectiveCamera& GetPerspectiveCamera() { return m_Camera; }

        /**
         * @brief Move the orbit target and update the camera.
         * @param target Target position in world space.
         */
        void SetTarget(const glm::vec3& target);

        /**
         * @brief Set the orbit distance within the configured limits.
         * @param distance Requested distance from the target in world units.
         */
        void SetDistance(float distance);

        /**
         * @brief Set the viewing angle around the orbit target.
         * @param yawDegrees Finite horizontal angle in degrees.
         * @param pitchDegrees Finite elevation in degrees, clamped to [-89, 89].
         */
        void SetOrbitAngles(float yawDegrees, float pitchDegrees);

        /**
         * @brief Center the camera on finite bounds and fit them within its field of view.
         *
         * Invalid bounds or an unsupported fitting distance are logged and leave the camera
         * unchanged.
         *
         * @param min Finite minimum corner; each component must not exceed max.
         * @param max Finite maximum corner.
         */
        void FrameBounds(const glm::vec3& min, const glm::vec3& max);

        /**
         * @brief Set the orbit input sensitivity.
         * @param sensitivity Degrees of orbit rotation per pixel.
         */
        void SetOrbitSensitivity(const float sensitivity) { m_OrbitSensitivity = sensitivity; }

        /**
         * @brief Set the pan input sensitivity.
         * @param sensitivity Pan scale per pixel, multiplied by orbit distance.
         */
        void SetPanSensitivity(const float sensitivity) { m_PanSensitivity = sensitivity; }

        /**
         * @brief Set the zoom input sensitivity.
         * @param sensitivity Distance change per scroll tick.
         */
        void SetZoomSensitivity(const float sensitivity) { m_ZoomSensitivity = sensitivity; }

        /** @brief Get the orbit target in world space. */
        const glm::vec3& GetTarget() const { return m_Target; }

        /** @brief Get the orbit distance in world units. */
        float GetDistance() const { return m_Distance; }

      protected:
        bool HandleFramebufferResize(const FramebufferResizeEvent& event) override;

      private:
        bool HandleMouseScrolled(const MouseScrolledEvent& event);
        bool HandleMouseMoved(const MouseMovedEvent& event);
        bool HandleMouseButtonPressed(const MouseButtonPressedEvent& event);
        bool HandleMouseButtonReleased(const MouseButtonReleasedEvent& event);

        void UpdateCameraFromOrbit();

        PerspectiveCamera m_Camera;

        // Orbit state
        glm::vec3 m_Target = { 0.0f, 0.0f, 0.0f };
        float m_Distance = 10.0f;
        float m_Yaw = -90.0f;      // Degrees, orbit angle in XZ plane
        float m_Pitch = 0.0f;      // Degrees, orbit angle above/below horizontal

        // Sensitivity
        float m_OrbitSensitivity = 0.25f;   // Degrees per pixel
        float m_PanSensitivity = 0.01f;     // Units per pixel (scaled by distance)
        float m_ZoomSensitivity = 1.0f;     // Distance units per scroll tick

        // Distance limits
        float m_MinDistance = 0.5f;
        float m_MaxDistance = 500.0f;

        // Mouse drag state
        bool m_OrbitActive = false;
        bool m_PanActive = false;
        glm::vec2 m_LastMousePos{};
        bool m_FirstMouse = true;
    };
}
