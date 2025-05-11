#pragma once

#include <Engine/Event/Event.h>

template <typename T>
concept DerivedFromEvent = std::is_base_of_v<Elixir::Event, T>;

template <DerivedFromEvent T>
struct std::formatter<T> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const T& event, FormatContext& ctx) const
    {
        return formatter<string>::format(event.ToString(), ctx);
    }
};