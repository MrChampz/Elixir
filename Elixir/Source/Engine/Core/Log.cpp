#include "epch.h"
#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Elixir
{
    Ref<spdlog::logger> Log::s_CoreLogger;
    Ref<spdlog::logger> Log::s_ClientLogger;

    void Log::Init()
    {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.emplace_back(CreateRef<spdlog::sinks::stdout_color_sink_mt>());
        sinks.emplace_back(CreateRef<spdlog::sinks::basic_file_sink_mt>("Elixir.log"));

        spdlog::set_pattern("%^[%T] %n: %v%$");

        s_CoreLogger = CreateRef<spdlog::logger>("ENGINE", begin(sinks), end(sinks));
        spdlog::register_logger(s_CoreLogger);
        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->flush_on(spdlog::level::trace);

        s_ClientLogger = CreateRef<spdlog::logger>("APP", begin(sinks), end(sinks));
        spdlog::register_logger(s_ClientLogger);
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);

        EE_CORE_INFO("Initialized log system!")
    }
}
