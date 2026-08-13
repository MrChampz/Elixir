#pragma once

#include <RmlUi/Core/SystemInterface.h>

#include <chrono>

struct GLFWcursor;
struct GLFWwindow;

class RmlUiSystemInterface final : public Rml::SystemInterface
{
public:
    explicit RmlUiSystemInterface(void* nativeWindow);
    ~RmlUiSystemInterface() override;

    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
    void SetMouseCursor(const Rml::String& cursorName) override;

private:
    std::chrono::steady_clock::time_point m_StartTime;
    GLFWwindow* m_Window = nullptr;
    GLFWcursor* m_PointerCursor = nullptr;
    GLFWcursor* m_CrossCursor = nullptr;
    GLFWcursor* m_TextCursor = nullptr;
    GLFWcursor* m_HorizontalResizeCursor = nullptr;
    GLFWcursor* m_VerticalResizeCursor = nullptr;
};
