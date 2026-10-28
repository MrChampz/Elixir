#include "epch.h"
#include "AnimationEvaluation.h"

#include <Engine/Animation/AnimationLayer.h>

namespace Elixir
{
    /* SAnimationNodeState */

    AnimationLayer* SAnimationNodeState::GetLayer(const std::string_view name) const
    {
        const auto* sources = std::get_if<SAnimationLayerSources>(&Sources);
        if (!sources)
        {
            return PrimaryPlayback
                ? PrimaryPlayback->GetLayer(name)
                : nullptr;
        }

        const auto separator = name.find('/');
        if (separator != std::string_view::npos)
        {
            const auto parent = name.substr(0, separator);

            for (const auto& layer : sources->Layers)
                if (layer->GetName() == parent)
                    return layer->GetPlayer().GetPlayback().GetLayer(name.substr(separator + 1));
        }

        for (const auto& layer : sources->Layers)
            if (layer->GetName() == name)
                return layer.get();

        AnimationLayer* result = nullptr;

        for (const auto& layer : sources->Layers)
        {
            if (auto* child = layer->GetPlayer().GetPlayback().GetLayer(name))
            {
                if (result) return nullptr;
                result = child;
            }
        }

        return result;
    }

    size_t SAnimationNodeState::GetLayerCount() const
    {
        const auto* sources = std::get_if<SAnimationLayerSources>(&Sources);

        if (!sources)
        {
            return PrimaryPlayback
                ? PrimaryPlayback->GetLayerCount()
                : 0;
        }

        size_t count = 0;
        for (const auto& layer : sources->Layers)
            count += std::max(size_t(1), layer->GetPlayer().GetPlayback().GetLayerCount());

        return count;
    }

    std::string_view SAnimationNodeState::GetCurrentState() const
    {
        if (!StateName.empty())
            return StateName;

        return PrimaryPlayback
            ? PrimaryPlayback->GetCurrentState()
            : std::string_view{};
    }

    bool SAnimationNodeState::IsTransitioning() const
    {
        if (Transitioning)
            return true;

        const auto* sources = std::get_if<SAnimationLayerSources>(&Sources);
        if (!sources) return false;

        const auto transitioning = std::ranges::any_of(
            sources->Layers,
            [](const auto& layer) { return layer->GetPlayer().IsTransitioning(); }
        );

        return transitioning;
    }
}
