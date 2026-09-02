#pragma once

namespace Elixir::Materials { class MaterialRegistry; }
namespace Elixir::Aether { class System; }

namespace Elixir::Aether::Effect
{
    using namespace Elixir::Materials;

    /**
     * @brief Resolves effect-authored material data into emitter material instances.
     *
     * The resolver bridges Aether::Effect authoring data and the application-wide
     * material system. It creates materials for serialized SMaterialDescription
     * values, retrieves default materials when no description is present, and
     * assigns an instance to each unresolved emitter.
     *
     * The resolver does not compile materials or create GPU render proxies.
     * MaterialSystem performs those operations during System compilation.
     *
     * @note The referenced MaterialRegistry must outlive this resolver.
     */
    class ELIXIR_API MaterialResolver final
    {
    public:
        /**
         * @brief Creates a resolver backed by the application material registry.
         *
         * @param registry Registry used to find, register, and retrieve materials.
         */
        explicit MaterialResolver(MaterialRegistry& registry);

        /**
         * @brief Resolves material instances for every emitter in an effect system.
         *
         * Emitters that already have a material instance keep their explicit
         * selection. For other emitters, the resolver creates or reuses an
         * effect-authored material when SMaterialDescription exists; otherwise, it
         * assigns the default material for the emitter render mode.
         *
         * @param system Effect system whose emitters require material instances.
         * @return True when every emitter has a resolved material instance.
         * @return False when a material cannot be created, registered, or assigned.
         */
        bool Resolve(const System& system) const;

    private:
        MaterialRegistry& m_Registry;
    };
}