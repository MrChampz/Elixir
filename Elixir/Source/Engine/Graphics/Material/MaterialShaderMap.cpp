#include "epch.h"
#include "MaterialShaderMap.h"

namespace Elixir
{
    MaterialShaderMap::MaterialShaderMap(KeyBuilder keyBuilder, Compiler compiler)
        : m_KeyBuilder(keyBuilder ? std::move(keyBuilder) : MaterialCompiler::BuildContentKey)
        , m_Compiler(compiler ? std::move(compiler) : MaterialCompiler::CompileToSpirv)
    {
    }

    MaterialShaderMap& MaterialShaderMap::Get()
    {
        static MaterialShaderMap map;
        return map;
    }

    std::optional<MaterialCompiler::SCompiled> MaterialShaderMap::GetOrCompile(const MaterialGraph& graph)
    {
        const std::optional<Key> key = m_KeyBuilder(graph);
        if (!key)
        {
            EE_CORE_ERROR("MaterialShaderMap could not build a content key.")
            return std::nullopt;
        }

        std::shared_ptr<SEntry> entry;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            const auto existing = m_Entries.find(*key);
            if (existing == m_Entries.end())
            {
                entry = std::make_shared<SEntry>();
                m_Entries.emplace(*key, entry);
            }
            else
            {
                entry = existing->second;
                entry->Ready.wait(lock, [&] { return entry->State != EState::Compiling; });
                return entry->Compiled;
            }
        }

        std::optional<MaterialCompiler::SCompiled> compiled;
        try
        {
            compiled = m_Compiler(graph);
        }
        catch (const std::exception& error)
        {
            EE_CORE_ERROR("MaterialShaderMap compiler failed: {}", error.what())
        }
        catch (...)
        {
            EE_CORE_ERROR("MaterialShaderMap compiler failed with an unknown exception.")
        }
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            entry->Compiled = compiled;
            entry->State = compiled ? EState::Ready : EState::Failed;
            // Compilation failures are shared with current waiters but not retained;
            // a later edit/retry can attempt the same key again.
            if (!compiled)
            {
                const auto current = m_Entries.find(*key);
                if (current != m_Entries.end() && current->second == entry)
                    m_Entries.erase(current);
            }
        }
        entry->Ready.notify_all();
        return compiled;
    }

    size_t MaterialShaderMap::EntryCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Entries.size();
    }
}
