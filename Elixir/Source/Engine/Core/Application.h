#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/Window.h>
#include <Engine/Core/Timer.h>
#include <Engine/Core/FrameProfiler.h>
#include <Engine/Core/Executor/Executor.h>
#include <Engine/Event/Event.h>
#include <Engine/Event/WindowEvent.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/GUI/Manager.h>

namespace Elixir
{
    namespace GUI { class TextBlock; }
    namespace Aether { class Manager; }

    namespace Materials
    {
        class MaterialSystem;
        class MaterialRegistry;
    }

    class PostProcessor;
}

namespace Elixir
{
    using namespace Elixir::Materials;

    class ELIXIR_API Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();

        virtual void OnGUI(Timestep frameTime) {}

        // Runs on the application thread before its render task is queued.
        // Implementations must not record GPU commands from this method.
        virtual void Prepare(Timestep frameTime) {}

        /**
         * @brief Configure animations before the system dispatches their worker tasks.
         * @param frameTime Elapsed application frame time.
         * @return Animation step in seconds; return zero to evaluate without advancing time.
         * @pre Runs on the game thread while earlier render tasks may still be executing.
         * Do not record GPU commands from this method.
         */
        virtual float PrepareAnimations(Timestep frameTime) { return frameTime.GetSeconds(); }

        /**
         * @brief Consume animation results after every submitted worker task has finished.
         * @param frameTime Elapsed application frame time.
         * @param succeeded True if every participating animation updated successfully.
         * @pre Runs on the game thread before the next render task is queued.
         * Do not record GPU commands from this method.
         */
        virtual void OnAnimationsUpdated(Timestep frameTime, bool succeeded) {}

        /**
         * @brief Record world rendering directly on the rendering thread.
         * @param frameTime Elapsed application frame time captured for this callback.
         * @pre Consume retained animation results instead of reading mutable playback state.
         */
        virtual void Render(Timestep frameTime) {}
        virtual void OnEvent(Event& event);

        const Window* GetWindow() const { return m_Window.get(); }

        MaterialSystem& GetMaterialSystem();
        const MaterialSystem& GetMaterialSystem() const;

        MaterialRegistry& GetMaterialRegistry();
        const MaterialRegistry& GetMaterialRegistry() const;

        Aether::Manager& GetAetherManager();
        const Aether::Manager& GetAetherManager() const;

        static Application& Get() { return *s_Application; }

    protected:
        bool OnWindowClose(WindowCloseEvent& event);
        bool OnWindowResize(WindowResizeEvent& event);
        bool OnFramebufferResize(const FramebufferResizeEvent& event);

        // Creates the application-owned HDR target used by world renderers.
        void CreateSceneTarget(const Extent3D& extent);

        Executor& m_Executor;
        Scope<Window> m_Window;
        Scope<GraphicsContext> m_GraphicsContext;

        Ref<Image> m_SceneTarget;

        Scope<ShaderLoader> m_ShaderLoader;
        Scope<GUI::Manager> m_GUIManager;

        Scope<MaterialSystem> m_MaterialSystem;
        Scope<MaterialRegistry> m_MaterialRegistry;

        Scope<Aether::Manager> m_AetherManager;

        Ref<PostProcessor> m_PostProcessor;

        Timer m_Timer;
        FrameProfiler m_Profiler;

        // Top-left FPS / frame-time overlay, refreshed a few times a second.
        Ref<GUI::TextBlock> m_StatsText;
        float m_StatsAccumulator = 0.0f;

        bool m_Running = true;
        bool m_Minimized = false;

    private:
        static Application* s_Application;
    };

    // NOTE: To be defined in the client
    extern Application* CreateApplication();
}
