#pragma once

#include <Engine/Graphics/Shader/ShaderBackend.h>

#include <glm/glm.hpp>

namespace Elixir
{
    class Window;
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

        virtual bool Prepare() = 0;
        virtual void Submit() = 0;
        virtual void Present() = 0;

        virtual void WaitDeviceIdle() = 0;

        virtual void SetClearColor(const glm::vec4& color) = 0;
        virtual void Clear() = 0;

        virtual void Resize(Extent2D extent) = 0;

        //        virtual void Dispatch(
        //            const Ref<CommandBuffer>& cmd,
        //            const Ref<Pipeline>& pipeline,
        //            uint32_t groupCountX,
        //            uint32_t groupCountY,
        //            uint32_t groupCountZ = 1
        //        ) = 0;

        virtual void FlushCommandBuffer(CommandBuffer& cmd) const = 0;

        Ref<CommandBuffer> CreateCommandBuffer();

        [[nodiscard]] EGraphicsAPI GetAPI() const { return m_API; }

        const Window* GetWindow() const { return m_Window; }
        const Scope<ShaderBackend>& GetShaderBackend() const { return m_ShaderBackend; }

        Ref<CommandBuffer> GetCommandBuffer() const { return m_CommandBuffers[m_FrameNumber % FRAMES]; }
        uint32_t GetFrameNumber() const { return m_FrameNumber; }

        virtual void SetVSyncEnabled(const bool enabled) { m_VSyncEnabled = enabled; }
        bool IsVSyncEnabled() const { return m_VSyncEnabled; }

        Ref<Texture2D> GetRenderTarget() const { return m_RenderTarget; }

        virtual Extent2D GetSwapchainExtent() const = 0;

        static Scope<GraphicsContext> Create(EGraphicsAPI api, const Window* window);

      protected:
        explicit GraphicsContext(const EGraphicsAPI api, const Window* window)
            : m_API(api), m_Window(window), m_CommandBuffers(FRAMES)
        {
            EE_PROFILE_ZONE_SCOPED()
        }

      private:
        virtual void CreateRenderTarget() = 0;

      protected:
        EGraphicsAPI m_API;

        const Window* m_Window;

        Ref<Texture2D> m_RenderTarget;

        std::vector<Ref<CommandBuffer>> m_CommandBuffers;
        uint32_t m_FrameNumber = 0;

        Scope<ShaderBackend> m_ShaderBackend = nullptr;

        bool m_VSyncEnabled = false;
    };
}