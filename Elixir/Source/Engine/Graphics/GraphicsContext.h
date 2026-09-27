#pragma once

#include <Engine/Graphics/Shader/ShaderBackend.h>

#include <glm/glm.hpp>

namespace Elixir
{
    class Executor;
    class Window;
    class Image;
    class CommandBuffer;
    class Pipeline;

    enum class EGraphicsAPI
    {
        Vulkan
    };

    class ELIXIR_API GraphicsContext
    {
      public:
        static constexpr uint32_t FRAMES = 2;

        virtual ~GraphicsContext() = default;

        virtual void Init() = 0;
        virtual void Shutdown() = 0;

        virtual void ProcessEvent(Event& event) = 0;

        virtual void RenderFrame(std::function<void()> callback) = 0;
        virtual void DrainRenderQueue() = 0;

        virtual void SetClearColor(const glm::vec4& color) = 0;
        virtual void Clear() = 0;

        virtual void Resize(Extent2D extent) = 0;

        /**
         * Returns a secondary command buffer that can be used to record GPU commands.
         *
         * Each call to this method returns a different CommandBuffer — either newly
         * created or recycled from a pool.
         *
         * The returned command buffer must be enqueued for submission using
         * @ref EnqueueSecondaryCommandBuffer
         *
         * All enqueued secondary command buffers are submitted together by a primary
         * command buffer.
         *
         * @return A secondary command buffer.
         */
        virtual Ref<CommandBuffer> GetSecondaryCommandBuffer() const = 0;
        virtual Ref<CommandBuffer> GetUploadCommandBuffer() const = 0;
        virtual void EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>& cmd) const = 0;

        /**
         * Block until the GPU has finished all submitted work.
         */
        virtual void WaitDeviceIdle() const {}

        EGraphicsAPI GetAPI() const { return m_API; }

        const Window* GetWindow() const { return m_Window; }
        float GetDPIScale() const;

        const Scope<ShaderBackend>& GetShaderBackend() const { return m_ShaderBackend; }

        /**
         * The number of frames being processed at a concurrent time. Double buffering.
         * @return the number of frames.
         */
        uint32_t GetFramesInFlight() const { return m_FramesInFlight; }

        /**
         * Returns the number of frames rendered since the app started.
         * @return the number of frames since app start.
         */
        uint32_t GetFrameNumber() const { return m_FrameNumber; }

        /**
         * Returns the index of the current frame.
         * @return the index of the current frame.
         */
        uint32_t GetFrameIndex() const { return m_FrameNumber % m_FramesInFlight; }

        virtual void SetVSyncEnabled(const bool enabled) { m_VSyncEnabled = enabled; }
        bool IsVSyncEnabled() const { return m_VSyncEnabled; }

        /** Returns the frame's color attachment as an image resource. */
        Ref<Image> GetRenderTarget() const { return m_RenderTarget; }
        
        /** Returns the frame's depth/stencil attachment as an image resource. */
        Ref<Image> GetDepthStencilRenderTarget() const { return m_DepthStencilRenderTarget; }

        virtual Extent3D GetSwapchainExtent() const = 0;

        static Scope<GraphicsContext> Create(EGraphicsAPI api, Executor* executor, const Window* window);

      protected:
        explicit GraphicsContext(const EGraphicsAPI api, const Window* window)
            : m_API(api), m_Window(window)
        {
            EE_PROFILE_ZONE_SCOPED()
        }

      private:
        virtual void CreateRenderTargets() = 0;

      protected:
        uint32_t m_FramesInFlight = FRAMES;
        uint32_t m_FrameNumber = 0;

        EGraphicsAPI m_API;
        const Window* m_Window;
        Ref<Image> m_RenderTarget;
        Ref<Image> m_DepthStencilRenderTarget;
        Scope<ShaderBackend> m_ShaderBackend = nullptr;

        bool m_VSyncEnabled = false;
    };
}
