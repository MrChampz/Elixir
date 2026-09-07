#pragma once

#include <Engine/Materials/Compilation/Compiler.h>

namespace Elixir::Materials::Rendering
{
    using namespace Compilation;

    /**
     * @brief Stores render-ready data for one material instance.
     *
     * The proxy copies resolved parameter values into slots defined by the compiled
     * material. It is valid only for the material and instance revisions used to
     * create it.
     */
    class ELIXIR_API MaterialRenderProxy final
    {
    public:
        /**
         * @brief Creates render-ready data for a material instance.
         * @param material Compiled material that defines parameter slots.
         * @param instance Material instance that provides resolved parameter values.
         * @return A render proxy, or null if the material is stale or a parameter
         * cannot be resolved.
         * @pre @p material was compiled from `instance.GetParent()`.
         * @pre The compiled material revision matches the parent material revision.
         */
        static Ref<const MaterialRenderProxy> Create(
            Ref<const SCompiledMaterial> material,
            const MaterialInstance& instance
        );

        /**
         * @brief Returns the compiled material that defines the parameter slots.
         * @return Read-only compiled material.
         */
        const Ref<const SCompiledMaterial> GetCompiledMaterial() const
        {
            return m_CompiledMaterial;
        }

        /**
         * @brief Returns the source instance revision.
         * @return Instance revision used to create this proxy.
         */
        uint32_t GetInstanceRevision() const { return m_InstanceRevision; }

        /**
         * @brief Returns resolved scalar and vector values by compiled slots.
         *
         * Scalar values use the X component. Vector values use all components.
         *
         * @return Read-only value slots.
         */
        const std::vector<glm::vec4>& GetValues() const { return m_Values; }

        /**
         * @brief Returns resolved textures by compiled slot.
         * @return Read-only texture slots.
         */
        const std::vector<Ref<Texture>>& GetTextures() const { return m_Textures; }

    private:
        Ref<const SCompiledMaterial> m_CompiledMaterial;
        uint32_t m_InstanceRevision = 0;
        std::vector<glm::vec4> m_Values;
        std::vector<Ref<Texture>> m_Textures;
    };
}