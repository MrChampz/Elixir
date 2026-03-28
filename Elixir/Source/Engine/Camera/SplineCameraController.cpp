#include "epch.h"
#include "SplineCameraController.h"

namespace Elixir
{
    SplineCameraController::SplineCameraController(float aspectRatio, float fov)
        : m_Camera(fov, aspectRatio, 0.1f, 1000.0f)
    {
    }

    // =========================================================================
    // Keyframe Authoring
    // =========================================================================

    void SplineCameraController::AddKeyframe(const SCameraKeyframe& keyframe)
    {
        m_Keyframes.push_back(keyframe);
    }

    void SplineCameraController::ClearKeyframes()
    {
        m_Keyframes.clear();
        Stop();
    }

    // =========================================================================
    // Playback Control
    // =========================================================================

    void SplineCameraController::Play()
    {
        if (m_Keyframes.size() < 2) return;

        if (m_State == EPlaybackState::Stopped)
            m_CurrentTime = 0.0f;

        m_State = EPlaybackState::Playing;
    }

    void SplineCameraController::Pause()
    {
        if (m_State == EPlaybackState::Playing)
            m_State = EPlaybackState::Paused;
    }

    void SplineCameraController::Stop()
    {
        m_State = EPlaybackState::Stopped;
        m_CurrentTime = 0.0f;

        if (!m_Keyframes.empty())
            ApplySplineState(0.0f);
    }

    float SplineCameraController::GetProgress() const
    {
        if (m_Duration <= 0.0f) return 0.0f;
        return glm::clamp(m_CurrentTime / m_Duration, 0.0f, 1.0f);
    }

    void SplineCameraController::Seek(float normalizedTime)
    {
        if (m_Keyframes.size() < 2) return;

        const float t = glm::clamp(normalizedTime, 0.0f, 1.0f);
        m_CurrentTime = t * m_Duration;
        ApplySplineState(t);
    }

    // =========================================================================
    // CameraController Interface
    // =========================================================================

    void SplineCameraController::OnUpdate(Timestep deltaTime)
    {
        if (m_State != EPlaybackState::Playing) return;
        if (m_Keyframes.size() < 2) return;

        m_CurrentTime += deltaTime.GetSeconds() * m_PlaybackSpeed;

        if (m_CurrentTime >= m_Duration)
        {
            if (m_Looping)
            {
                m_CurrentTime = std::fmod(m_CurrentTime, m_Duration);
            }
            else
            {
                m_CurrentTime = m_Duration;
                m_State = EPlaybackState::Stopped;
            }
        }

        const float t = glm::clamp(m_CurrentTime / m_Duration, 0.0f, 1.0f);
        ApplySplineState(t);
    }

    void SplineCameraController::OnEvent(Event& /*event*/)
    {
        // Intentionally empty.
        // A spline camera is time-driven, not input-driven.
        // This is the point: same CameraController interface, completely
        // different driving logic.
    }

    void SplineCameraController::OnResize(uint32_t width, uint32_t height)
    {
        if (height == 0) return;

        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        m_Camera.SetAspectRatio(aspectRatio);
    }

    // =========================================================================
    // Spline Evaluation
    // =========================================================================

    void SplineCameraController::ApplySplineState(float t)
    {
        const size_t numKeyframes = m_Keyframes.size();
        EE_CORE_ASSERT(numKeyframes >= 2, "Spline requires at least 2 keyframes!")

        // Map normalized t [0, 1] to a segment index and local parameter.
        //
        // With N keyframes there are (N-1) segments. We find which segment
        // we're in and compute a local t within that segment.
        const size_t numSegments = numKeyframes - 1;
        const float scaledT = t * static_cast<float>(numSegments);
        const size_t segment = glm::min(
            static_cast<size_t>(scaledT),
            numSegments - 1
        );
        const float localT = scaledT - static_cast<float>(segment);

        // Catmull-Rom needs four points: P0, P1, P2, P3.
        // P1 and P2 are the segment endpoints.
        // For boundary segments, we clamp / mirror the phantom points.
        const size_t i1 = segment;
        const size_t i2 = segment + 1;
        const size_t i0 = (segment == 0) ? 0 : segment - 1;
        const size_t i3 = glm::min(segment + 2, numKeyframes - 1);

        const auto& kf0 = m_Keyframes[i0];
        const auto& kf1 = m_Keyframes[i1];
        const auto& kf2 = m_Keyframes[i2];
        const auto& kf3 = m_Keyframes[i3];

        // Interpolate position, look-at target, and FOV independently
        const glm::vec3 position = CatmullRom(kf0.Position, kf1.Position, kf2.Position, kf3.Position, localT);
        const glm::vec3 lookAt   = CatmullRom(kf0.LookAtTarget, kf1.LookAtTarget, kf2.LookAtTarget, kf3.LookAtTarget, localT);
        const float fov          = CatmullRomScalar(kf0.FieldOfView, kf1.FieldOfView, kf2.FieldOfView, kf3.FieldOfView, localT);

        // Convert the look-at target into orientation angles for the camera
        const glm::vec3 direction = lookAt - position;
        const float dirLength = glm::length(direction);

        if (dirLength > 0.0001f)
        {
            float yaw, pitch;
            DirectionToYawPitch(direction / dirLength, yaw, pitch);
            m_Camera.SetOrientation(yaw, pitch);
        }

        m_Camera.SetFieldOfView(fov);
        m_Camera.SetPosition(position);
    }

    // =========================================================================
    // Math Utilities
    // =========================================================================

    glm::vec3 SplineCameraController::CatmullRom(
        const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2, const glm::vec3& p3,
        float t)
    {
        // Standard Catmull-Rom matrix form:
        //   q(t) = 0.5 * [ 1  t  t^2  t^3 ] * | 0   2   0   0 | * | P0 |
        //                                       | -1  0   1   0 |   | P1 |
        //                                       | 2  -5   4  -1 |   | P2 |
        //                                       | -1  3  -3   1 |   | P3 |
        const float t2 = t * t;
        const float t3 = t2 * t;

        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
        );
    }

    float SplineCameraController::CatmullRomScalar(float p0, float p1, float p2, float p3, float t)
    {
        const float t2 = t * t;
        const float t3 = t2 * t;

        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
        );
    }

    void SplineCameraController::DirectionToYawPitch(const glm::vec3& direction, float& outYaw, float& outPitch)
    {
        // Pitch: angle above/below the horizontal plane
        outPitch = glm::degrees(glm::asin(glm::clamp(direction.y, -1.0f, 1.0f)));

        // Yaw: angle in the horizontal (XZ) plane, measured from +X toward +Z
        outYaw = glm::degrees(glm::atan(direction.z, direction.x));
    }
}
