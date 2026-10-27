#include "epch.h"
#include "Sorter.h"

namespace Elixir
{
    void SectionSorter::SetContext(const SSortContext& context)
    {
        m_ModelView = context.View * context.Model;
    }

    float SectionSorter::CalculateDepth(const SMeshSection& section) const
    {
        const auto center = (section.LocalBounds.Min + section.LocalBounds.Max) * 0.5f;
        return -(m_ModelView * glm::vec4(center, 1.0f)).z;
    }
}
