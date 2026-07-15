#pragma once

#include <Engine/Graphics/Shader/ShaderBackend.h>

#include <glm/glm.hpp>

namespace Elixir
{
    class Executor;
    class Window;
    class DepthStencilImage;
    class Texture2D;
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
         * Blits the current render target into @p dst (a sampled texture), inserting
         * the barriers needed to read it in a subsequent pass. Used to grab the scene
         * colour for screen-space effects like refractive glass.
         */
        virtual void BlitToTexture(const Ref<CommandBuffer>& cmd, const Ref<Texture2D>& dst) const = 0;

        // Dear ImGui integration. The app builds a frame between BeginImGuiFrame()
        // and EndImGuiFrame() on the UI thread. RenderImGuiFrame() records the
        // completed frame over the render target on the render thread. No-ops on
        // backends without ImGui support.
        virtual void InitImGui() {}
        virtual void BeginImGuiFrame() {}
        virtual void EndImGuiFrame() {}
        virtual void RenderImGuiFrame() {}

        // Block until the GPU has finished all submitted work. Needed before
        // destroying resources (e.g. recreating pipelines) that in-flight frames
        // may still reference.
        virtual void WaitDeviceIdle() const {}

        [[nodiscard]] EGraphicsAPI GetAPI() const { return m_API; }

        const Window* GetWindow() const { return m_Window; }
        float GetDPIScale() const;

        const Scope<ShaderBackend>& GetShaderBackend() const { return m_ShaderBackend; }

        /**
         * The number of frames being processed at a concurrent time. Double buffering.
         * @return the number of frames.
         */
        [[nodiscard]] uint32_t GetFramesInFlight() const { return m_FramesInFlight; }

        /**
         * Returns the number of frames rendered since the app started.
         * @return the number of frames since app start.
         */
        [[nodiscard]] uint32_t GetFrameNumber() const { return m_FrameNumber; }

        /**
         * Returns the index of the current frame.
         * @return the index of the current frame.
         */
        [[nodiscard]] uint32_t GetFrameIndex() const { return m_FrameNumber % m_FramesInFlight; }

        virtual void SetVSyncEnabled(const bool enabled) { m_VSyncEnabled = enabled; }
        bool IsVSyncEnabled() const { return m_VSyncEnabled; }

        Ref<Texture2D> GetRenderTarget() const { return m_RenderTarget; }
        Ref<DepthStencilImage> GetDepthStencilRenderTarget() const { return m_DepthStencilRenderTarget; }

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
        Ref<Texture2D> m_RenderTarget;
        Ref<DepthStencilImage> m_DepthStencilRenderTarget;
        Scope<ShaderBackend> m_ShaderBackend = nullptr;

        bool m_VSyncEnabled = false;
    };
}
