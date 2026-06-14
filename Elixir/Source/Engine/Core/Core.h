#pragma once

#include <memory>

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
    #define EE_ASSERT(x, message, ...)                                                      \
        if (!(x))                                                                           \
        {                                                                                   \
            EE_ERROR(message __VA_OPT__(,) __VA_ARGS__)                                     \
            DEBUG_BREAK()                                                                   \
        }
    #define EE_CORE_ASSERT(x, message, ...)                                                 \
        if (!(x))                                                                           \
        {                                                                                   \
            EE_CORE_ERROR(message __VA_OPT__(,) __VA_ARGS__)                                \
            DEBUG_BREAK()                                                                   \
        }
#else
    #define EE_ASSERT(x, message, ...)
    #define EE_CORE_ASSERT(x, message, ...)
#endif // EE_ENABLE_ASSERTS

#if defined(_MSC_VER)
    #define DEBUG_BREAK() __debugbreak();
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
    #define DEBUG_BREAK() __builtin_debugtrap();
#else
    #define DEBUG_BREAK() raise(SIGABRT);
#endif // _MSC_VER

#define BIT(x) (1 << x)

#define EE_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)
#define EE_BIND_EVENT_FN_STATIC(fn) std::bind(&fn, std::placeholders::_1)

#define GENERATE_ENUM_CLASS_OPERATORS(EnumClass)                                    \
inline bool operator&(EnumClass lhs, EnumClass rhs)                                 \
{                                                                                   \
	using T = std::underlying_type_t<EnumClass>;                                    \
    return (static_cast<T>(lhs) & static_cast<T>(rhs)) ==  static_cast<T>(rhs);     \
}                                                                                   \
                                                                                    \
inline EnumClass operator|(EnumClass lhs, EnumClass rhs)							\
{																					\
	using T = std::underlying_type_t<EnumClass>; 								    \
    return static_cast<EnumClass>(static_cast<T>(lhs) | static_cast<T>(rhs));       \
}																					\
                                                                                    \
inline EnumClass& operator|=(EnumClass& lhs, EnumClass rhs)							\
{																					\
	lhs = lhs | rhs;													            \
	return lhs;															            \
}

template <typename, typename = void>
struct HasGetHashParams : std::false_type {};

template <typename T>
struct HasGetHashParams<T, std::void_t<decltype(std::declval<T>().GetHashParams())>>
    : std::true_type {};

#define GENERATE_HASH_FUNCTION(T)                                                                \
namespace std                                                                                    \
{                                                                                                \
    template <>                                                                                  \
    struct hash<T> {                                                                             \
        size_t operator()(const T& obj) const noexcept                                           \
        {                                                                                        \
            static_assert(HasGetHashParams<T>::value, #T " must define GetHashParams()");        \
            return Elixir::Hash::HashValues(obj.GetHashParams());                                                                            \
        }                                                                                        \
    };                                                                                           \
}

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

    using Byte = std::byte;

    namespace Hash
    {
        inline void HashCombine(std::size_t& seed, std::size_t value)
        {
            // Similar to boost::hash_combine
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }

        template <typename T, typename = void>
        struct HasToString : std::false_type {};

        template <typename T>
        struct HasToString<T, std::void_t<decltype(std::declval<const T&>().ToString())>>
            : std::true_type {};

        template <typename T>
        std::size_t HashValue(const T& value)
        {
            if constexpr (requires { std::hash<T>{}(value); })
            {
                return std::hash<T>{}(value);
            }
            else if constexpr (HasToString<T>::value)
            {
                return std::hash<std::string>{}(value.ToString());
            }
            else
            {
                static_assert(sizeof(T) == 0, "Type is not hashable and has no ToString()");
            }
        }

        template <typename... T>
        std::size_t HashValues(const T&... values)
        {
            std::size_t seed = 0;
            (HashCombine(seed, HashValue(values)), ...);
            return seed;
        }

        template <typename... T>
        std::size_t HashValues(const std::tuple<T...>& values)
        {
            return std::apply([](const T&... args)
                {
                    return HashValues(args...);
                },
                values
            );
        }
    }
}