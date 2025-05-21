#include "epch.h"
#include "GLFWWindow.h"

#include <Engine/Event/ApplicationEvent.h>
#include <Engine/Event/WindowEvent.h>
#include <Engine/Event/MouseEvent.h>
#include <Engine/Event/KeyEvent.h>

namespace Elixir
{
    Scope<Window> Window::Create(const WindowProps& props)
    {
        return CreateScope<Platform::GLFW::GLFWWindow>(props);
    }

    namespace Platform::GLFW
    {
        static bool s_GLFWInitialized = false;

        static void GLFWErrorCallback(int error, const char* description)
        {
            EE_CORE_ERROR("GLFW Error ({0}): {1}.", error, description)
        }

        GLFWWindow::GLFWWindow(const WindowProps& props)
            : m_Window{}
        {
            Init(props);
        }

        GLFWWindow::~GLFWWindow()
        {
            Shutdown();
        }

        void GLFWWindow::OnUpdate()
        {
            glfwPollEvents();
        }

        void GLFWWindow::SetTitle(const std::string& title)
        {
            m_Data.Title = title;

            if (s_GLFWInitialized)
            {
                glfwSetWindowTitle(m_Window, title.c_str());
            }
        }

        void GLFWWindow::ShowFPSAndFrameTime(const int fps, const Timestep frameTime)
        {
            auto title = std::stringstream();
            title << m_Data.Title;
            title << " | Frame time: " << std::format("{:.3f}ms", frameTime.GetMilliseconds());
            title << " | FPS: " << std::to_string(fps);

            if (s_GLFWInitialized)
            {
                glfwSetWindowTitle(m_Window, title.str().c_str());
            }
        }

        void GLFWWindow::Init(const WindowProps& props)
        {
            m_Data.Title = props.Title;
            m_Data.Width = props.Width;
            m_Data.Height = props.Height;

            EE_CORE_INFO("Creating window {0} ({1}, {2}).", props.Title,
                         props.Width, props.Height)

            if (!s_GLFWInitialized)
            {
                const int result = glfwInit();
                EE_CORE_ASSERT(result, "Could not initialize GLFW!")

                glfwSetErrorCallback(GLFWErrorCallback);

                s_GLFWInitialized = true;
            }

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            m_Window = glfwCreateWindow(
                (int)m_Data.Width,
                (int)m_Data.Height,
                m_Data.Title.c_str(),
                nullptr,
                nullptr
            );

            glfwSetWindowUserPointer(m_Window, &m_Data);

            // Set GLFW callbacks

            glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
            {
                auto& data = *(WindowData*)glfwGetWindowUserPointer(window);
                data.Width = width;
                data.Height = height;

                WindowResizeEvent event(width, height);
                data.EventCallback(event);
            });

            glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
            {
                WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
                WindowCloseEvent event;
                data.EventCallback(event);
            });

            glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scanCode, int action, int mods)
            {
                WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

                switch (action)
                {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
                default:
                    EE_CORE_ERROR("Unknown GLFW key action!")
                    break;
                }
            });

            glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keyCode)
            {
                WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
                KeyTypedEvent event((int)keyCode);
                data.EventCallback(event);
            });

            glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
            {
                WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

                switch (action)
                {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                default:
                    EE_CORE_ERROR("Unknown GLFW mouse button action!")
                    break;
                }
            });

            glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
            {
                WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
                MouseScrolledEvent event((float)xOffset, (float)yOffset);
                data.EventCallback(event);
            });

            glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
            {
                WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
                MouseMovedEvent event((float)xPos, (float)yPos);
                data.EventCallback(event);
            });
        }

        void GLFWWindow::Shutdown()
        {
            glfwDestroyWindow(m_Window);
            glfwTerminate();
        }
    }
}