#include "epch.h"
#include "GLFWPlatform.h"

#include "Engine/Core/Window.h"

#include <GLFW/glfw3.h>

namespace Elixir
{
    Scope<Platform> Platform::Create(const Window* window)
    {
        return CreateScope<GLFW::GLFWPlatform>(window);
    }

    namespace GLFW
    {
        int GetGLFWCursor(const ECursorShape shape)
        {
            switch (shape)
            {
                case ECursorShape::Arrow:            return GLFW_ARROW_CURSOR;
                case ECursorShape::IBeam:            return GLFW_IBEAM_CURSOR;
                case ECursorShape::Crosshair:        return GLFW_CROSSHAIR_CURSOR;
                case ECursorShape::Hand:             return GLFW_HAND_CURSOR;
                case ECursorShape::NotAllowed:       return GLFW_NOT_ALLOWED_CURSOR;
                case ECursorShape::ResizeHorizontal: return GLFW_HRESIZE_CURSOR;
                case ECursorShape::ResizeVertical:   return GLFW_VRESIZE_CURSOR;
                default:
                    EE_CORE_WARN("Unknown cursor shape!")
                    return GLFW_ARROW_CURSOR;
            }
        }

        GLFWPlatform::GLFWPlatform(const Window* window)
            : m_Window(window)
        {
            EE_PROFILE_ZONE_SCOPED()
        }

        void GLFWPlatform::SetPreviousCursorShape()
        {
            SetCursorShape(m_PrevCursor);
        }

        void GLFWPlatform::SetCursorShape(const ECursorShape shape)
        {
            m_PrevCursor = m_Cursor;
            m_Cursor = shape;
            const auto cursor = glfwCreateStandardCursor(GetGLFWCursor(shape));
            glfwSetCursor((GLFWwindow*)m_Window->GetHandle(), cursor);
        }

        std::string GLFWPlatform::GetClipboardText()
        {
            return glfwGetClipboardString(nullptr);
        }

        void GLFWPlatform::SetClipboardText(const std::string& text)
        {
            glfwSetClipboardString(nullptr, text.c_str());
        }
    }
}