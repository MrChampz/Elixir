#pragma once

#include "VFXDocument.h"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace EditorVFX
{
    /** @brief Describes an emitter's estimated particle visibility in seconds. */
    struct TimelineInterval
    {
        float Start = 0;
        float End = 0;
    };

    /** @brief Resolves numeric literals and emitter-local or system parameter references. */
    inline float TimelineNumber(const Value* value, const Value& emitter, const Value& system, int depth = 0)
    {
        if (!value || depth > 8) return 0;
        if (value->Type == EValueType::String && value->Text.starts_with('$'))
        {
            const auto name = value->Text.substr(1);
            const auto local = emitter.Find("parameters");
            const auto global = system.Find("parameters");
            const auto parameter = local && local->Find(name) ? local->Find(name) : global ? global->Find(name) : nullptr;
            return TimelineNumber(parameter, emitter, system, depth + 1);
        }
        float number = 0;
        const auto result = std::from_chars(value->Text.data(), value->Text.data() + value->Text.size(), number);
        return result.ec == std::errc{} && result.ptr == value->Text.data() + value->Text.size() && std::isfinite(number) ? std::max(0.0f, number) : 0;
    }

    /**
     * @brief Estimates visible intervals from spawn rate, periodic bursts, trigger delays, and lifetime.
     * @note Trigger timing is derived from the source's periodic bursts. Dynamic lifetime inputs
     * cannot be predicted here and use the preview duration as their visibility window.
     */
    inline std::vector<TimelineInterval> EmitterTimeline(const Value& system, size_t index, float duration)
    {
        const auto emitters = system.Find("emitters");
        if (!emitters || index >= emitters->Array.size() || duration <= 0) return {};
        const auto& emitter = emitters->Array[index];
        if (!emitter.Enabled()) return {};
        if (TimelineNumber(emitter.Find("spawnRate"), emitter, system) > 0) return {{0, duration}};
        const auto burst = emitter.Find("burst");
        if (!burst || TimelineNumber(burst->Find("count"), emitter, system) <= 0) return {};
        float period = TimelineNumber(burst->Find("interval"), emitter, system);
        float delay = 0;
        if (const auto trigger = emitter.Find("trigger"))
        {
            const auto sourceName = trigger->String("source");
            const auto source = std::find_if(emitters->Array.begin(), emitters->Array.end(),
                [&](const Value& candidate) { return candidate.String("name") == sourceName; });
            if (source == emitters->Array.end() || !source->Enabled() || source->Find("trigger")) return {};
            const auto sourceBurst = source->Find("burst");
            if (!sourceBurst || TimelineNumber(sourceBurst->Find("count"), *source, system) <= 0) return {};
            period = TimelineNumber(sourceBurst->Find("interval"), *source, system);
            delay = TimelineNumber(trigger->Find("delay"), emitter, system);
        }
        if (period <= 0) return {};
        float lifetime = duration;
        if (const auto modules = emitter.Find("spawnModules"))
            for (const auto& module : modules->Array)
                if (module.Enabled() && module.String("type") == "SetLifetime")
                {
                    const auto maximum = module.Find("max");
                    const float resolved = TimelineNumber(maximum, emitter, system);
                    if (resolved > 0) lifetime = resolved;
                }
        std::vector<TimelineInterval> intervals;
        // Sub-frame bursts form a continuous visibility window and need no individual rectangles.
        if (period <= std::max(lifetime, 1.0f / 60.0f)) return period + delay < duration ?
            std::vector<TimelineInterval>{{period + delay, duration}} : intervals;
        for (float start = period + delay; start < duration; start += period)
            intervals.push_back({start, std::min(duration, start + lifetime)});
        return intervals;
    }
}
