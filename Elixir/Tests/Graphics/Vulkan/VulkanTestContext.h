#pragma once

#include <Engine/Core/Executor/Executor.h>
#include <Engine/Core/Window.h>
#include <Engine/Graphics/GraphicsContext.h>

using namespace Elixir;

class VulkanTestContext final
{
public:
    static VulkanTestContext& Get()
    {
        static VulkanTestContext context;
        return context;
    }

    GraphicsContext* GetGraphicsContext() const { return m_Context.get(); }

private:
    VulkanTestContext()
    {
        Memory::s_Malloc = CreateScope<SystemMalloc>();
        m_Window = Window::Create();
        m_Context = GraphicsContext::Create(
            EGraphicsAPI::Vulkan,
            &Executor::Get(),
            m_Window.get()
        );
        m_Context->Init();
    }

    Scope<Window> m_Window;
    Scope<GraphicsContext> m_Context;
};
