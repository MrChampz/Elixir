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
#include <Engine/Logging/Log.h>
#include <Engine/Logging/Formatters.h>
#include <Engine/Instrumentation/Profiler.h>

#define _USE_MATH_DEFINES
#include <cmath>

#ifdef EE_PROFILE
    #include <tracy/Tracy.hpp>
#endif

#ifdef EE_PLATFORM_WINDOWS
    #ifndef NOMINMAX
        # define NOMINMAX
    #endif

    #include <windows.h>
#endif