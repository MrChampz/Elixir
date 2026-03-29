#pragma once

#include <Engine/Camera/CameraController.h>
#include <Engine/Camera/PerspectiveCamera.h>

namespace Elixir
{
    /**
     * @brief A single point on a camera spline path.
     *
     * Each keyframe defines where the camera is, what it looks at, and its
     * field of view at a given normalized time [0, 1] along the path.
     */
    struct SCameraKeyframe
    {
        glm::vec3 Position;
        glm::vec3 LookAtTarget;
        float FieldOfView;
    };

    enum class EPlaybackState : uint8_t
    {
        Stopped, Playing, Paused
    };

    /**
     * @brief Drives a perspective camera along a Catmull-Rom spline path.
     *
     * Designed for cinematics, cutscenes, and flythrough sequences. The camera
     * position, look-at target, and FOV are each interpolated independently
     * along smooth Catmull-Rom splines that pass through every keyframe.
     */
    class ELIXIR_API SplineCameraController final : public CameraController
    {
      public:
        static constexpr float INITIAL_FOV = 60.0f;

        explicit SplineCameraController(float aspectRatio);

        void AddKeyframe(const SCameraKeyframe& keyframe);
        void ClearKeyframes();

        size_t GetKeyframeCount() const { return m_Keyframes.size(); }
        const SCameraKeyframe& GetKeyframe(const size_t index) const { return m_Keyframes[index]; }

        void Play();
        void Pause();
        void Stop();

        void SetDuration(const float seconds) { m_Duration = glm::max(seconds, 0.001f); }
        void SetPlaybackSpeed(const float speed) { m_PlaybackSpeed = speed; }
        void SetLooping(const bool loop) { m_Looping = loop; }

        float GetDuration() const { return m_Duration; }
        float GetPlaybackSpeed() const { return m_PlaybackSpeed; }
        bool IsLooping() const { return m_Looping; }
        EPlaybackState GetPlaybackState() const { return m_State; }

        /** Current progress through the path, normalized [0, 1]. */
        float GetProgress() const;

        /** Manually seek to a normalized position [0, 1] on the spline. */
        void Seek(float normalizedTime);

        void Update(Timestep deltaTime) override;
        void ProcessEvent(Event& event) override;

        Camera& GetCamera() override { return m_Camera; }
        const Camera& GetCamera() const override { return m_Camera; }
        PerspectiveCamera& GetPerspectiveCamera() { return m_Camera; }

      private:
        bool HandleFramebufferResize(const FramebufferResizeEvent& event) override;
        void ApplySplineState(float t);

        /**
         * @brief Catmull-Rom interpolation between four points.
         *
         * Given points P0, P1, P2, P3 and parameter t in [0,1],
         * returns a point on the curve segment between P1 and P2.
         *
         * The tension parameter alpha defaults to 0.5 (centripetal),
         * which avoids cusps and self-intersections.
         */
        static glm::vec3 CatmullRom(
            const glm::vec3& p0,
            const glm::vec3& p1,
            const glm::vec3& p2,
            const glm::vec3& p3,
            float t
        );

        static float CatmullRomScalar(float p0, float p1, float p2, float p3, float t);

        /**
         * @brief Converts a direction vector to yaw/pitch Euler angles (degrees).
         */
        static void DirectionToYawPitch(const glm::vec3& direction, float& yaw, float& pitch);

        PerspectiveCamera m_Camera;

        std::vector<SCameraKeyframe> m_Keyframes;

        EPlaybackState m_State = EPlaybackState::Stopped;
        float m_CurrentTime = 0.0f;     // Seconds elapsed
        float m_Duration = 5.0f;        // Total path duration in seconds
        float m_PlaybackSpeed = 1.0f;
        bool m_Looping = false;
    };
}