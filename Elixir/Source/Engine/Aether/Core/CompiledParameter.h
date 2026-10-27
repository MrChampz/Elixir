#pragma once

#include <Engine/Aether/Core/ParameterStore.h>

#include <span>

namespace Elixir::Aether::Core
{
    /** @brief Stores a parameter's default data and location in the compiled buffer. */
    struct SCompiledParameter : SParameter
    {
        /** Offset from the instance's parameter base, in four-byte words. */
        uint32_t WordOffset = 0;
    };

    /**
     * @brief Assign consecutive native-width word offsets to authored parameters.
     * @param parameters Authored parameters in compiled-table order.
     * @return Compiled parameters, or an empty vector for an empty input, invalid type, or
     * overflow.
     */
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

    /**
     * @brief Pack compiled defaults as raw words without changing their bit patterns.
     * @param parameters Compiled parameters with consecutive native-width word offsets.
     * @return Packed words, or an empty vector for an empty input, invalid type, offset, or
     * overflow.
     */
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
