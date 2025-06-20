#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <format>
#include <cstdint>

#include <string>
#include <sstream>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <deque>
#include <span>

#include <Engine/Core/UUID.h>
#include <Engine/Core/Types.h>
#include <Engine/Core/Memory.h>
#include <Engine/Event/EventFormatter.h>
#include <Engine/Logging/Log.h>
#include <Engine/Logging/Formatters.h>
#include <Engine/Instrumentation/Profiler.h>

#define _USE_MATH_DEFINES
#include <cmath>

#ifdef EE_PROFILE
    #include <tracy/Tracy.hpp>
#endif

#ifdef EE_PLATFORM_WINDOWS
    #define NO_RPC
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif