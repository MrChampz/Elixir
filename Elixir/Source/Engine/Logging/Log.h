#pragma once

#include <Engine/Event/Event.h>

#include <spdlog/spdlog.h>

namespace Elixir
{
    class ELIXIR_API Log
    {
      public:
        static void Init();

        static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        static Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

      private:
        static Ref<spdlog::logger> s_CoreLogger;
        static Ref<spdlog::logger> s_ClientLogger;
    };
}

// Core log macros
#define EE_CORE_TRACE(...)    ::Elixir::Log::GetCoreLogger()->trace(__VA_ARGS__);
#define EE_CORE_INFO(...)     ::Elixir::Log::GetCoreLogger()->info(__VA_ARGS__);
#define EE_CORE_WARN(...)     ::Elixir::Log::GetCoreLogger()->warn(__VA_ARGS__);
#define EE_CORE_ERROR(...)    ::Elixir::Log::GetCoreLogger()->error(__VA_ARGS__);
#define EE_CORE_FATAL(...)    ::Elixir::Log::GetCoreLogger()->critical(__VA_ARGS__);

// Client log macros
#define EE_TRACE(...)         ::Elixir::Log::GetClientLogger()->trace(__VA_ARGS__);
#define EE_INFO(...)          ::Elixir::Log::GetClientLogger()->info(__VA_ARGS__);
#define EE_WARN(...)          ::Elixir::Log::GetClientLogger()->warn(__VA_ARGS__);
#define EE_ERROR(...)         ::Elixir::Log::GetClientLogger()->error(__VA_ARGS__);
#define EE_FATAL(...)         ::Elixir::Log::GetClientLogger()->critical(__VA_ARGS__);