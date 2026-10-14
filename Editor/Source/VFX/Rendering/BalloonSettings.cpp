#include "BalloonSettings.h"
#include <charconv>
#include <cmath>
#include <algorithm>

namespace EditorVFX
{
    bool BalloonSettings::IsValid() const
    {
        return std::isfinite(Radius) && Radius > 0 && std::isfinite(Height) && Height > 0 &&
            std::isfinite(Tilt) && Tilt >= 0 && Tilt <= 1 &&
            std::isfinite(ColorVariation) && ColorVariation >= 0 && ColorVariation <= 1 &&
            std::all_of(Color.begin(), Color.end(), [](float value) { return std::isfinite(value) && value >= 0; }) &&
            Color[3] <= 1;
    }

    bool ParseBalloonRenderers(const Value& effect, std::vector<std::optional<BalloonSettings>>& output, std::string& error)
    {
        const auto emitters = effect.Find("emitters");
        if (!emitters || emitters->Type != EValueType::Array)
        { error = "An effect needs an emitters array."; return false; }
        std::vector<std::optional<BalloonSettings>> parsed;
        for (const auto& emitter : emitters->Array)
        {
            const auto fail = [&error, &emitter](const std::string& reason)
            { error = "Balloon Renderer on '" + emitter.String("name") + "': " + reason; return false; };
            const auto config = emitter.Find("balloonRenderer");
            if (!config) { parsed.emplace_back(); continue; }
            if (config->Type != EValueType::Object || emitter.String("renderMode") != "Strand")
                return fail("requires an object on a Strand emitter.");
            BalloonSettings settings;
            if (const auto enabled = config->Find("enabled"))
            {
                if (enabled->Type != EValueType::Boolean) return fail("enabled must be a boolean.");
                settings.Enabled = enabled->Text == "true";
            }
            const auto number = [config](const char* key, auto& target)
            {
                const auto value = config->Find(key);
                if (!value) return true;
                if (value->Type != EValueType::Number) return false;
                const auto result = std::from_chars(value->Text.data(), value->Text.data() + value->Text.size(), target);
                return result.ec == std::errc{} && result.ptr == value->Text.data() + value->Text.size();
            };
            if (!number("radius", settings.Radius) || !number("height", settings.Height) ||
                !number("tilt", settings.Tilt) || !number("colorVariation", settings.ColorVariation) || !number("seed", settings.Seed))
                return fail("dimensions, tilt, variation, and seed must be supported numeric values.");
            if (const auto color = config->Find("color"))
            {
                if (color->Type != EValueType::Array || color->Array.size() != 4)
                    return fail("color must contain four numbers.");
                for (size_t index = 0; index < 4; ++index)
                {
                    const auto& component = color->Array[index];
                    if (component.Type != EValueType::Number) return fail("color must contain four numbers.");
                    const auto result = std::from_chars(component.Text.data(), component.Text.data() + component.Text.size(), settings.Color[index]);
                    if (result.ec != std::errc{} || result.ptr != component.Text.data() + component.Text.size())
                        return fail("color must contain finite numbers.");
                }
            }
            if (!settings.IsValid()) return fail("invalid dimensions, tilt, color variation or color.");
            parsed.push_back(settings);
        }
        output = std::move(parsed);
        error.clear();
        return true;
    }
}
