#pragma once

#include <Engine/Graphics/Material/MaterialCompiler.h>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace Elixir
{
    // Process-wide map of content-addressed material shader artifacts. It owns no
    // Vulkan resources: Shader objects keep descriptor bindings and parameter buffers
    // local to their renderer, while the expensive DXC result is shared by every
    // material slot and renderer that requests the same graph permutation and target.
    class ELIXIR_API MaterialShaderMap
    {
      public:
        using Key = uint64_t;
        using KeyBuilder = std::function<std::optional<Key>(
            const MaterialGraph&, const SMaterialShaderPermutation&)>;
        using Compiler = std::function<std::optional<MaterialCompiler::SCompiled>(
            const MaterialGraph&, const SMaterialShaderPermutation&)>;

        MaterialShaderMap(KeyBuilder keyBuilder = {}, Compiler compiler = {});

        static MaterialShaderMap& Get();

        // Safe to call concurrently from workers. The first caller compiles a missing
        // key; other callers wait on that worker's entry instead of launching DXC.
        std::optional<MaterialCompiler::SCompiled> GetOrCompile(
            const MaterialGraph& graph, const SMaterialShaderPermutation& permutation);

        [[nodiscard]] size_t EntryCount() const;

      private:
        enum class EState : uint8_t { Compiling, Ready, Failed };
        struct SEntry
        {
            EState State = EState::Compiling;
            std::optional<MaterialCompiler::SCompiled> Compiled;
            std::condition_variable Ready;
        };

        KeyBuilder m_KeyBuilder;
        Compiler m_Compiler;
        mutable std::mutex m_Mutex;
        std::unordered_map<Key, std::shared_ptr<SEntry>> m_Entries;
    };
}
