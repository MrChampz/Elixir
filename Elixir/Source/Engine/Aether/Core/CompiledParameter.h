#pragma once

#include <Engine/Aether/Core/ParameterStore.h>

#include <span>

namespace Elixir::Aether::Core
{
    /** @brief Stores a parameter's default data and location in the compiled buffer. */
    struct SCompiledParameter : SParameter
    {
        /** @brief Offset from the instance's parameter base, in four-byte words. */
        uint32_t WordOffset = 0;
    };

    /** @brief Copies authored parameters and assigns consecutive offsets at their native sizes. */
    inline std::vector<SCompiledParameter> CompileParameters(std::span<const SParameter> parameters)
    {
        std::vector<SCompiledParameter> compiled;
        compiled.reserve(parameters.size());
        uint32_t offset = 0;
        for (const auto& parameter : parameters)
        {
            const auto count = GetParameterWordCount(parameter.Type);
            if (count == 0 || offset > UINT32_MAX - count)
            {
                EE_CORE_ERROR("Aether compiled parameter has an invalid type or exceeds the word range.");
                return {};
            }
            compiled.push_back({parameter, offset});
            offset += count;
        }
        return compiled;
    }

    /** @brief Packs compiled defaults as raw words, preserving float and uint bit patterns. */
    inline std::vector<uint32_t> PackParameterValues(std::span<const SCompiledParameter> parameters)
    {
        uint32_t offset = 0;
        for (const auto& parameter : parameters)
        {
            const auto count = GetParameterWordCount(parameter.Type);
            if (parameter.WordOffset != offset || count == 0 || offset > UINT32_MAX - count)
            {
                EE_CORE_ERROR("Aether compiled parameter range is invalid.");
                return {};
            }
            offset += count;
        }
        std::vector<uint32_t> words(offset);
        for (const auto& parameter : parameters)
            std::memcpy(words.data() + parameter.WordOffset, &parameter.Value.x,
                GetParameterWordCount(parameter.Type) * sizeof(uint32_t));
        return words;
    }
}
