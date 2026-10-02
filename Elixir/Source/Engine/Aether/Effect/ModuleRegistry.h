#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

#include <simdjson.h>

namespace Elixir::Aether::Effect
{
    /** @brief Creates one module from its JSON object. */
    using ModuleFactory = Scope<Modules::Module> (*)(ModuleParseContext&, simdjson::ondemand::object&);

    /** @brief Registers and creates particle modules by their serialized type names. */
    class ELIXIR_API ModuleRegistry final
    {
    public:
        /** @brief Registers a factory. Type names must be unique for the process lifetime. */
        static bool Register(std::string_view typeName, ModuleFactory factory);

        /**
         * @brief Creates a registered module from its serialized fields.
         * @return Null when the type is unknown or field validation fails.
         * Validation errors are available through context.GetError().
         */
        static Scope<Modules::Module> Create(
            std::string_view typeName,
            ModuleParseContext& context,
            simdjson::ondemand::object& object
        );
    };

/** @brief Registers a factory once per type; invoke in one source file, outside a function. */
#define AETHER_REGISTER_MODULE(TypeName, Factory) \
    namespace { const bool TypeName##Registered = ::Elixir::Aether::Effect::ModuleRegistry::Register(#TypeName, Factory); }
}
