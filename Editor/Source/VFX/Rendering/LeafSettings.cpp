#include "LeafSettings.h"
#include <charconv>
#include <cmath>
#include <algorithm>

namespace EditorVFX
{
    bool LeafSettings::IsValid() const
    {
        return Interval >= 1 && Interval <= 96 && std::isfinite(Length) && Length > 0 &&
            std::isfinite(Width) && Width > 0 && std::isfinite(MaturationSegments) && MaturationSegments > 0 &&
            std::all_of(Color.begin(), Color.end(), [](float value) { return std::isfinite(value) && value >= 0; }) &&
            Color[3] <= 1 && uint32_t(Shape) <= uint32_t(ELeafShape::Lanceolate);
    }

    uint32_t LeafSettings::GetLeafCount(uint32_t pointCount) const
    {
        return Interval && pointCount > 2 ? (pointCount - 2) / Interval : 0;
    }

    bool ParseLeafRenderers(const Value& effect, std::vector<std::optional<LeafSettings>>& output, std::string& error)
    {
        const auto emitters = effect.Find("emitters");
        if (!emitters || emitters->Type != EValueType::Array)
        { error = "An effect needs an emitters array."; return false; }
        std::vector<std::optional<LeafSettings>> parsed;
        for (const auto& emitter : emitters->Array)
        {
            const auto fail = [&error, &emitter](const std::string& reason)
            { error = "Leaf Renderer on '" + emitter.String("name") + "': " + reason; return false; };
            const auto config = emitter.Find("leafRenderer");
            if (!config) { parsed.emplace_back(); continue; }
            if (config->Type != EValueType::Object || emitter.String("renderMode") != "Strand")
                return fail("requires an object on a Strand emitter.");
            LeafSettings settings;
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
            if (!number("interval", settings.Interval) || !number("length", settings.Length) ||
                !number("width", settings.Width) || !number("maturationSegments", settings.MaturationSegments))
                return fail("dimensions and interval must be numeric values in their supported ranges.");
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
            if (const auto shape = config->Find("shape"))
            {
                if (shape->Type != EValueType::String) return fail("shape must be Ivy or Lanceolate.");
                if (shape->Text == "Ivy") settings.Shape = ELeafShape::Ivy;
                else if (shape->Text == "Lanceolate") settings.Shape = ELeafShape::Lanceolate;
                else return fail("shape must be Ivy or Lanceolate.");
            }
            if (!settings.IsValid()) return fail("invalid dimensions, interval, maturation distance or color.");
            parsed.push_back(settings);
        }
        output = std::move(parsed);
        error.clear();
        return true;
    }
}
