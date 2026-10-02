#include "epch.h"
#include "ModuleRegistry.h"

namespace Elixir::Aether::Modules
{
    namespace
    {
        using Registry = std::unordered_map<std::string, ModuleFactory>;

        Registry& GetRegistry()
        {
            // The registry intentionally survives static destruction. Module
            // registration can run from another translation unit during shutdown.
            static auto* registry = new Registry();
            return *registry;
        }
    }

    bool ModuleRegistry::Register(const std::string_view typeName, const ModuleFactory factory)
    {
        EE_CORE_ASSERT(factory, "Aether module factory must be valid.")
        const auto [_, inserted] = GetRegistry().emplace(std::string(typeName), factory);
        EE_CORE_ASSERT(inserted, "Aether module type '{}' was registered twice.", typeName)
        return inserted;
    }

    Scope<Module> ModuleRegistry::Create(
        const std::string_view typeName,
        simdjson::ondemand::object& object
    )
    {
        const auto it = GetRegistry().find(std::string(typeName));
        return it == GetRegistry().end() ? nullptr : it->second(object);
    }
}
