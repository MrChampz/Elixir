#pragma once

#include <Engine/Material/MaterialRenderProxy.h>

namespace Elixir
{
    /**
     * @brief Defines the GPU data layout for one resolved material.
     *
     * Material templates share this layout with the shader interface.
     */
    struct alignas(16) SMaterialFrameData
    {
        /** @brief Numeric material values, stored by material value slot. */
        std::array<glm::vec4, 32> Values{};

        /** @brief Bindless texture indices, stored by material texture slot. */
        std::array<uint32_t, 32> TextureIndices{};
    };

    /**
     * @brief Stores resolved material data for one frame submission.
     *
     * Each material proxy is assigned one stable table index. Values and textures
     * beyond the fixed GPU layout are not included.
     */
    class ELIXIR_API MaterialFrameTable final
    {
    public:
        /** @brief Resolves a texture into an index usable by the GPU. */
        using TextureIndexResolver = std::function<uint32_t(const Ref<Texture>&)>;

        /**
         * @brief Creates an empty material frame table.
         * @param capacity Maximum number of material entries.
         * @param fallbackTextureIndex Index used for missing texture bindings.
         * @param resolver Function that resolves material textures to GPU indices.
         * @pre resolver is valid.
         */
        MaterialFrameTable(
            uint32_t capacity,
            uint32_t fallbackTextureIndex,
            TextureIndexResolver resolver
        );

        /**
         * @brief Adds a material to the table when capacity is available.
         *
         * Adding an existing material returns its current index.
         *
         * @param material Resolved material data to add.
         * @return The material table index, or no value when the table is full.
         */
        std::optional<uint32_t> Add(const MaterialRenderProxy& material);

        /**
         * @brief Finds the table index assigned to a material.
         * @param material Resolved material data to find.
         * @return The material table index, or no value when it is not in the table.
         */
        std::optional<uint32_t> Find(const MaterialRenderProxy& material) const;

        /** @brief Gets the GPU material data in table-index order. */
        const std::vector<SMaterialFrameData>& GetData() const { return m_Data; }

        /** @brief Gets the number of materials stored in the table. */
        uint32_t GetCount() const { return static_cast<uint32_t>(m_Data.size()); }

    private:
        /** @brief Builds the GPU data for a resolved material. */
        SMaterialFrameData BuildData(const MaterialRenderProxy& material) const;

        uint32_t m_Capacity = 0;
        uint32_t m_FallbackTextureIndex = 0;
        TextureIndexResolver m_TextureIndexResolver;
        std::unordered_map<const MaterialRenderProxy*, uint32_t> m_Indices;
        std::vector<SMaterialFrameData> m_Data;
    };
}