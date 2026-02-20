#pragma once

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Definitions.h>

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan
{
    class VulkanGraphicsContext;

    struct SDescriptorPoolSizeRatio
    {
        VkDescriptorType Type;
        float Ratio;
    };

    class VulkanBaseDescriptorPool
    {
      public:
        virtual ~VulkanBaseDescriptorPool();

        uint32_t GetMaxSets() const { return m_MaxSets; }
        VkDescriptorPool GetVulkanDescriptorPool() const { return m_Pool; }

      protected:
        explicit VulkanBaseDescriptorPool(
            const VulkanGraphicsContext& context,
            const uint32_t maxSets
        ) : m_MaxSets(maxSets), m_GraphicsContext(&context) {}

        virtual void InitPool(
            std::vector<VkDescriptorPoolSize> sizes,
            VkDescriptorPoolCreateFlags flags
        );
        virtual void DestroyPool() const;

        uint32_t m_MaxSets;

        VkDescriptorPool m_Pool = VK_NULL_HANDLE;
        const VulkanGraphicsContext* m_GraphicsContext;
    };

    class VulkanDescriptorPool final : public VulkanBaseDescriptorPool
    {
      public:
        VulkanDescriptorPool(
            const VulkanGraphicsContext& context,
            uint32_t maxSets,
            std::span<SDescriptorPoolSizeRatio> ratios
        );

        VkDescriptorSet Allocate(VkDescriptorSetLayout layout) const;
        void FreeDescriptorSet(VkDescriptorSet set) const;
        void FreeDescriptorSets(const std::vector<VkDescriptorSet>& sets) const;
        void Reset() const;
    };

    class VulkanBindlessDescriptorPool final : public VulkanBaseDescriptorPool
    {
      public:
        static constexpr uint32_t MAX_TEXTURES = 16384;
        static constexpr uint32_t MAX_SAMPLERS = 256;
        static constexpr uint32_t MAX_CONSTANT_BUFFERS = 4096;

        explicit VulkanBindlessDescriptorPool(
            const VulkanGraphicsContext& context,
            uint32_t maxTextures = MAX_TEXTURES,
            uint32_t maxSamplers = MAX_SAMPLERS,
            uint32_t maxConstantBuffers = MAX_CONSTANT_BUFFERS
        );

        ~VulkanBindlessDescriptorPool() override;

        VulkanBindlessDescriptorPool(const VulkanBindlessDescriptorPool&) = delete;
        VulkanBindlessDescriptorPool& operator=(const VulkanBindlessDescriptorPool&) = delete;
        VulkanBindlessDescriptorPool(VulkanBindlessDescriptorPool&&) = delete;
        VulkanBindlessDescriptorPool& operator=(VulkanBindlessDescriptorPool&&) = delete;

        /**
         * Flush dirty descriptors to the GPU.
         * This should be called after any updates to ensure changes are visible in shaders.
         */
        void FlushDescriptors();

        /**
         * Register a texture and get its handle in the bindless array.
         * If the texture is already registered, returns its handle.
         * @param texture The texture to register
         * @return A handle to the texture in the bindless array.
         */
        SResourceHandle RegisterTexture(const Ref<Texture>& texture);

        /**
         * Update the texture bind at a specific handle.
         * @param handle The handle to be updated.
         * @param texture The new texture.
         */
        void UpdateTexture(SResourceHandle handle, const Ref<Texture>& texture);

        /**
         * Unregister a texture by its handle.
         * This decrements the reference count and frees the slot if count reaches 0.
         * @param handle The handle to the texture to be unregistered.
         */
        void UnregisterTexture(SResourceHandle handle);

        /**
         * Get the handle for a registered texture.
         * @param texture The texture to look up.
         * @return The handle for the texture, or an invalid handle if not found.
         */
        SResourceHandle GetTextureHandle(const Ref<Texture>& texture) const;

        /**
         * Get the maximum number of textures.
         */
        uint32_t GetMaxTextures() const { return m_MaxTextures; }

        /**
         * Get the number of currently registered textures.
         */
        uint32_t GetTextureCount() const { return m_TextureCount; }

        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }

      private:
        std::vector<VkDescriptorSetLayoutBinding> GetDescriptorSetLayoutBindings() const;
        void CreateDescriptorSetLayout();
        void AllocateDescriptorSet();
        void InitTexturesStructures();

        void UpdateTextureDescriptors(const std::vector<Ref<Texture>>& textures) const;

        SResourceHandle AllocateTextureHandle();
        void FreeTextureHandle(SResourceHandle handle);

        uint32_t GetMaxDescriptorsByType(VkDescriptorType type) const;

        uint32_t m_MaxTextures;
        uint32_t m_MaxSamplers;
        uint32_t m_MaxConstantBuffers;

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

        struct STextureEntry
        {
            Ref<Texture> Texture;
            uint32_t RefCount = 0;
        };

        std::vector<STextureEntry> m_Textures;
        std::vector<Ref<Texture>> m_DirtyTextures;
        std::unordered_map<Texture*, SResourceHandle> m_TextureLookup;
        uint32_t m_NextFreeIndex = 0;
        std::vector<uint32_t> m_RecycledIndices;
        uint32_t m_TextureCount = 0;
        mutable std::mutex m_TextureMutex;
    };
}