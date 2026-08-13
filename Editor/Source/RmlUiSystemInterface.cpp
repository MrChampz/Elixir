#include "RmlUiSystemInterface.h"

#include <Engine/Logging/Log.h>
#include <GLFW/glfw3.h>

RmlUiSystemInterface::RmlUiSystemInterface(void* nativeWindow)
    : m_StartTime(std::chrono::steady_clock::now()),
      m_Window(static_cast<GLFWwindow*>(nativeWindow)),
      m_PointerCursor(glfwCreateStandardCursor(GLFW_HAND_CURSOR)),
      m_CrossCursor(glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR)),
      m_TextCursor(glfwCreateStandardCursor(GLFW_IBEAM_CURSOR)),
      m_HorizontalResizeCursor(glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR)),
      m_VerticalResizeCursor(glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR))
{
}

RmlUiSystemInterface::~RmlUiSystemInterface()
{
    glfwDestroyCursor(m_PointerCursor);
    glfwDestroyCursor(m_CrossCursor);
    glfwDestroyCursor(m_TextCursor);
    glfwDestroyCursor(m_HorizontalResizeCursor);
    glfwDestroyCursor(m_VerticalResizeCursor);
}

double RmlUiSystemInterface::GetElapsedTime()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_StartTime).count();
}

bool RmlUiSystemInterface::LogMessage(const Rml::Log::Type type, const Rml::String& message)
{
    switch (type)
    {
        case Rml::Log::LT_ERROR:
        case Rml::Log::LT_ASSERT:
            EE_CORE_ERROR("RmlUi: {}", message)
            break;
        case Rml::Log::LT_WARNING:
            EE_CORE_WARN("RmlUi: {}", message)
            break;
        case Rml::Log::LT_INFO:
            EE_CORE_INFO("RmlUi: {}", message)
            break;
        default:
            EE_CORE_TRACE("RmlUi: {}", message)
            break;
    }

    return true;
}

void RmlUiSystemInterface::SetMouseCursor(const Rml::String& cursorName)
{
    GLFWcursor* cursor = nullptr;
    if (cursorName == "pointer" || cursorName == "move" || cursorName == "resize")
        cursor = m_PointerCursor;
    else if (cursorName == "cross")
        cursor = m_CrossCursor;
    else if (cursorName == "text")
        cursor = m_TextCursor;
    else if (cursorName == "col-resize")
        cursor = m_HorizontalResizeCursor;
    else if (cursorName == "row-resize")
        cursor = m_VerticalResizeCursor;

    if (m_Window)
        glfwSetCursor(m_Window, cursor);
}
