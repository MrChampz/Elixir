#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <format>

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
#include <Engine/Core/Log.h>

#define _USE_MATH_DEFINES
#include <cmath>

#ifdef EE_PLATFORM_WINDOWS
#include <Windows.h>
#endif // EE_PLATFORM_WINDOWS