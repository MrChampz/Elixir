#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/Camera/Camera.h>

namespace Elixir
{
    class FramebufferResizeEvent;

    /**
     * @brief Abstract camera controller that bridges input events to camera transforms.
     *
     * Processes keyboard and mouse input to drive camera movement and rotation.
     * Derived classes implement behavior specific to orthographic or perspective cameras.
     */
    class ELIXIR_API CameraController
    {
      public:
        virtual ~CameraController() = default;

        virtual void Update(Timestep deltaTime) {}
        virtual void ProcessEvent(Event& event) {}

        virtual Camera& GetCamera() = 0;
        virtual const Camera& GetCamera() const = 0;

      protected:
        CameraController() = default;

        virtual bool HandleFramebufferResize(const FramebufferResizeEvent& event) = 0;
    };
}