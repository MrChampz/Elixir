#include "epch.h"
#include "AnimationMontage.h"

namespace Elixir
{
    Ref<AnimationMontage> AnimationMontage::Create(SAnimationMontageCreateInfo info)
    {
        if (info.Name.empty() ||
            !info.Sequence ||
            info.Sequence->GetDuration() <= 0 ||
            !std::isfinite(info.BlendIn) ||
            info.BlendIn < 0 ||
            !std::isfinite(info.BlendOut) ||
            info.BlendOut < 0)
        {
            EE_CORE_ERROR("Animation montage requires a name, a timed sequence, and valid fades.")
            return nullptr;
        }

        if (info.Sections.empty())
            info.Sections.push_back({.Name = "Default"});

        std::unordered_set<std::string> names;
        for (size_t i = 0; i < info.Sections.size(); ++i)
        {
            const auto& section = info.Sections[i];
            const auto [_, inserted] = names.insert(section.Name);

            if (section.Name.empty() || !inserted)
            {
                EE_CORE_ERROR("Montage section names must be nonempty and unique.")
                return nullptr;
            }

            if (!std::isfinite(section.StartTime) ||
                section.StartTime < 0 ||
                section.StartTime >= info.Sequence->GetDuration() ||
                (i == 0 && section.StartTime != 0) ||
                (i > 0 && section.StartTime <= info.Sections[i - 1].StartTime))
            {
                EE_CORE_ERROR("Montage sections must start at zero and increase within the clip.")
                return nullptr;
            }
        }

        for (const auto& section : info.Sections)
        {
            if (section.NextSection &&
                !section.NextSection->empty() &&
                !names.contains(*section.NextSection))
            {
                EE_CORE_ERROR("Montage section continuation '{}' does not exist.",
                    *section.NextSection)
                return nullptr;
            }
        }

        return Ref<AnimationMontage>(new AnimationMontage(std::move(info)));
    }

    AnimationMontage::AnimationMontage(SAnimationMontageCreateInfo info)
        : m_Info(std::move(info))
    {
        m_NextSections.reserve(m_Info.Sections.size());
        for (size_t i = 0; i < m_Info.Sections.size(); ++i)
        {
            const auto& next = m_Info.Sections[i].NextSection;
            if (next)
            {
                m_NextSections.push_back(FindSection(*next));
                continue;
            }

            const auto nextIndex = i + 1;
            const auto nextSection = nextIndex < m_Info.Sections.size()
                ? std::optional(nextIndex)
                : std::nullopt;

            m_NextSections.push_back(nextSection);
        }
    }

    std::optional<size_t> AnimationMontage::FindSection(const std::string_view name) const
    {
        for (size_t i = 0; i < m_Info.Sections.size(); ++i)
            if (m_Info.Sections[i].Name == name)
                return i;
        return std::nullopt;
    }

    float AnimationMontage::GetSectionEnd(const size_t index) const
    {
        if (index >= m_Info.Sections.size())
        {
            EE_CORE_ERROR("Montage section index is invalid.")
            return 0;
        }

        return index + 1 < m_Info.Sections.size()
            ? m_Info.Sections[index + 1].StartTime
            : m_Info.Sequence->GetDuration();
    }

    std::optional<size_t> AnimationMontage::GetNextSection(const size_t index) const
    {
        if (index >= m_NextSections.size())
        {
            EE_CORE_ERROR("Montage section index is invalid.")
            return std::nullopt;
        }

        return m_NextSections[index];
    }
}
