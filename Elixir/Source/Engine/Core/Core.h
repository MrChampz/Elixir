#pragma once

#include <memory>
#include <csignal>

#if defined(EE_PLATFORM_WINDOWS)
    #ifdef EE_BUILD_DLL
        #define ELIXIR_API __declspec(dllexport)
    #else
        #define ELIXIR_API __declspec(dllimport)
    #endif // EE_BUILD_DLL
#elif defined(EE_PLATFORM_MACOS)
    #define ELIXIR_API
#else
    #error Elixir only support Windows and MacOS!
#endif // EE_PLATFORM_WINDOWS

#ifdef EE_DEBUG
    #define EE_ENABLE_ASSERTS
#endif // EE_DEBUG

#ifdef EE_ENABLE_ASSERTS
    #define EE_ASSERT(x, message, ...) { if (!(x)) { std::string error = "Assertion failed: " + std::string(message); EE_ERROR(error, ##__VA_ARGS__); DEBUG_BREAK(); } }
    #define EE_CORE_ASSERT(x, message, ...) { if (!(x)) { std::string error = "Assertion failed: " + std::string(message); EE_CORE_ERROR(error, ##__VA_ARGS__); DEBUG_BREAK(); } }
#else
    #define EE_ASSERT(x, ...)
    #define EE_CORE_ASSERT(x, ...)
#endif // EE_ENABLE_ASSERTS

#if defined(_MSC_VER)
    #define DEBUG_BREAK() __debugbreak();
#elif defined(SIGTRAP)
    #define DEBUG_BREAK() raise(SIGTRAP);
#else
    #define DEBUG_BREAK() raise(SIGABRT);
#endif // _MSC_VER

#define BIT(x) (1 << x)

#define EE_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)
#define EE_BIND_EVENT_FN_STATIC(fn) std::bind(&fn, std::placeholders::_1)

namespace Elixir
{
    template <typename T>
    using Scope = std::unique_ptr<T>;

    template <typename T, typename ... Args>
    constexpr Scope<T> CreateScope(Args&& ... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    using Ref = std::shared_ptr<T>;

    template <typename T, typename ... Args>
    constexpr Ref<T> CreateRef(Args&& ... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    using Weak = std::weak_ptr<T>;

    using Byte = uint8_t;
}