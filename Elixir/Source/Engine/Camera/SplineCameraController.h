#pragma once

#include <Engine/Camera/CameraController.h>

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
        float FieldOfView = 60.0f;
    };

    enum class EPlaybackState : uint8_t
    {
        Stopped,
        Playing,
        Paused
    };

    /**
     * @brief Drives a perspective camera along a Catmull-Rom spline path.
     *
     * Designed for cinematics, cutscenes, and flythrough sequences. The camera
     * position, look-at target, and FOV are each interpolated independently
     * along smooth Catmull-Rom splines that pass through every keyframe.
     *
     * Catmull-Rom was chosen because:
     *  - The curve passes exactly through each control point (WYSIWYG editing)
     *  - C1 continuity gives smooth velocity transitions between segments
     *  - Only needs the two neighboring points plus one on each side
     *
     * The controller consumes no input events — it is purely time-driven.
     * This is the cleanest demonstration of why Camera and Controller are
     * separate: the same PerspectiveCamera that a PerspectiveCameraController
     * drives with WASD+mouse is driven here by a spline and a clock.
     *
     * Usage:
     * @code
     *     SplineCameraController cinematic(16.0f / 9.0f);
     *     cinematic.AddKeyframe({ { 0, 5, 10 }, { 0, 0, 0 }, 60.0f });
     *     cinematic.AddKeyframe({ { 10, 3, 0 }, { 0, 1, 0 }, 45.0f });
     *     cinematic.AddKeyframe({ { 0, 8, -10}, { 0, 0, 0 }, 60.0f });
     *     cinematic.SetDuration(8.0f);
     *     cinematic.Play();
     *
     *     // In your update loop:
     *     cinematic.OnUpdate(deltaTime);
     *     auto vp = cinematic.GetCamera().GetViewProjectionMatrix();
     * @endcode
     */
    class ELIXIR_API SplineCameraController final : public CameraController
    {
      public:
        explicit SplineCameraController(float aspectRatio, float fov = 60.0f);

        // -- Keyframe authoring -----------------------------------------------

        void AddKeyframe(const SCameraKeyframe& keyframe);
        void ClearKeyframes();

        [[nodiscard]] size_t GetKeyframeCount() const { return m_Keyframes.size(); }
        [[nodiscard]] const SCameraKeyframe& GetKeyframe(size_t index) const { return m_Keyframes[index]; }

        // -- Playback control -------------------------------------------------

        void Play();
        void Pause();
        void Stop();

        void SetDuration(float seconds) { m_Duration = glm::max(seconds, 0.001f); }
        void SetPlaybackSpeed(float speed) { m_PlaybackSpeed = speed; }
        void SetLooping(bool loop) { m_Looping = loop; }

        [[nodiscard]] float GetDuration() const { return m_Duration; }
        [[nodiscard]] float GetPlaybackSpeed() const { return m_PlaybackSpeed; }
        [[nodiscard]] bool IsLooping() const { return m_Looping; }
        [[nodiscard]] EPlaybackState GetPlaybackState() const { return m_State; }

        /** Current progress through the path, normalized [0, 1]. */
        [[nodiscard]] float GetProgress() const;

        /** Manually seek to a normalized position [0, 1] on the spline. */
        void Seek(float normalizedTime);

        // -- CameraController interface ---------------------------------------

        void OnUpdate(Timestep deltaTime) override;
        void OnEvent(Event& event) override;
        void OnResize(uint32_t width, uint32_t height) override;

        [[nodiscard]] Camera& GetCamera() override { return m_Camera; }
        [[nodiscard]] const Camera& GetCamera() const override { return m_Camera; }
        [[nodiscard]] PerspectiveCamera& GetPerspectiveCamera() { return m_Camera; }

      private:
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
            const glm::vec3& p0, const glm::vec3& p1,
            const glm::vec3& p2, const glm::vec3& p3,
            float t
        );

        static float CatmullRomScalar(float p0, float p1, float p2, float p3, float t);

        /**
         * @brief Converts a direction vector to yaw/pitch Euler angles (degrees).
         */
        static void DirectionToYawPitch(const glm::vec3& direction, float& outYaw, float& outPitch);

        PerspectiveCamera m_Camera;

        std::vector<SCameraKeyframe> m_Keyframes;

        EPlaybackState m_State = EPlaybackState::Stopped;
        float m_CurrentTime = 0.0f;     // Seconds elapsed
        float m_Duration = 5.0f;        // Total path duration in seconds
        float m_PlaybackSpeed = 1.0f;
        bool m_Looping = false;
    };
}
