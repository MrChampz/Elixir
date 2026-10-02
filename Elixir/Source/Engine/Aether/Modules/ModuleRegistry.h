#pragma once

#include <Engine/Aether/Modules/Module.h>

#include <simdjson.h>

namespace Elixir::Aether::Modules
{
    /** @brief Creates one module from its JSON object. */
    using ModuleFactory = Scope<Module> (*)(simdjson::ondemand::object&);

    /** @brief Registers and creates particle modules by their serialized type names. */
    class ELIXIR_API ModuleRegistry final
    {
    public:
        /** @brief Registers a factory. Type names must be unique for the process lifetime. */
        static bool Register(std::string_view typeName, ModuleFactory factory);

        /** @brief Creates a registered module, or returns null when the type is unknown. */
        static Scope<Module> Create(
            std::string_view typeName,
            simdjson::ondemand::object& object
        );
    };

#define AETHER_REGISTER_MODULE(TypeName, Factory) \
    namespace { const bool TypeName##Registered = ::Elixir::Aether::Modules::ModuleRegistry::Register(#TypeName, Factory); }
}
