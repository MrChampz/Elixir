#pragma once

#include <Engine/Graphics/Image.h>
#include <Engine/Graphics/Definitions.h>

#include <vulkan/vulkan.h>
#include <mutex>
#include <unordered_map>

namespace Elixir::Vulkan
{
    class VulkanGraphicsContext;

    struct SDescriptorPoolSizeRatio
    {
        VkDescriptorType Type;
        float Ratio;
    };

    class ELIXIR_API VulkanBaseDescriptorPool
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
        virtual void DestroyPool();

        uint32_t m_MaxSets;

        VkDescriptorPool m_Pool = VK_NULL_HANDLE;
        const VulkanGraphicsContext* m_GraphicsContext;
    };

    /**
     * @brief Allocates descriptor sets from pools that grow without invalidating existing
     * sets.
     */
    class ELIXIR_API VulkanDescriptorPool final : public VulkanBaseDescriptorPool
    {
      public:
        /** @brief Creates the first pool with the specified set and descriptor capacity. */
        VulkanDescriptorPool(
            const VulkanGraphicsContext& context,
            uint32_t maxSets,
            std::span<SDescriptorPoolSizeRatio> ratios
        );

        /**
         * @brief Releases all additional pools after their descriptor sets are no longer
         * in use.
         */
        ~VulkanDescriptorPool() override;

        /**
         * @brief Allocates a set, adding a pool when full; returns null on allocation failure.
         */
        VkDescriptorSet Allocate(VkDescriptorSetLayout layout) const;

        /** @brief Frees a set from the pool that owns it after GPU use has finished. */
        void FreeDescriptorSet(VkDescriptorSet set) const;

        /** @brief Frees sets from their owning pools after GPU use has finished. */
        void FreeDescriptorSets(const std::vector<VkDescriptorSet>& sets) const;

        /** @brief Invalidates all sets and resets every pool after GPU use has finished. */
        void Reset() const;

        /** @brief Returns the number of allocated pools, including the initial pool. */
        size_t GetPoolCount() const;

      private:
        // Attempts allocation from one pool and records the set's owner on success.
        VkResult AllocateFromPool(
            VkDescriptorPool pool,
            VkDescriptorSetLayout layout,
            VkDescriptorSet& set
        ) const;

        std::vector<VkDescriptorPoolSize> m_PoolSizes;
        mutable std::vector<VkDescriptorPool> m_AdditionalPools;
        mutable std::unordered_map<VkDescriptorSet, VkDescriptorPool> m_SetOwners;
        mutable std::mutex m_Mutex;
    };

    class VulkanBindlessDescriptorPool final : public VulkanBaseDescriptorPool
    {
      public:
        static constexpr uint32_t MAX_IMAGES   = 16384;
        static constexpr uint32_t MAX_SAMPLERS = 256;

        explicit VulkanBindlessDescriptorPool(
            const VulkanGraphicsContext& context,
            uint32_t maxImages = MAX_IMAGES,
            uint32_t maxSamplers = MAX_SAMPLERS
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
        ELIXIR_API void FlushDescriptors();

        /**
         * Register a sampled image and get its handle in the bindless array.
         * If the image is already registered, returns its handle.
         * @param image The image to register
         * @return An invalid handle when the image cannot be sampled or the pool is full.
         */
        SResourceHandle RegisterImage(const Ref<Image>& image);

        /**
         * Update the image binding at a specific handle.
         * @param handle The handle to be updated.
         * @param image The new image, which must not be registered at another handle.
         */
        void UpdateImage(SResourceHandle handle, const Ref<Image>& image);

        /**
         * Unregister an image by its handle.
         * This decrements the reference count and frees the slot if count reaches 0.
         * @param handle The handle to the image to be unregistered.
         */
        void UnregisterImage(SResourceHandle handle);

        /**
         * Get the handle for a registered image.
         * @param image The image to look up.
         * @return The handle for the image, or an invalid handle if not found.
         */
        SResourceHandle GetImageHandle(const Ref<Image>& image) const;

        /**
         * Get the image associated with a handle.
         * @param handle The handle to look up.
         * @return The image associated with the handle, or nullptr if not found.
         */
        ELIXIR_API Ref<Image> GetImage(SResourceHandle handle) const;

        /**
         * Get the maximum number of images.
         */
        uint32_t GetMaxImages() const { return m_MaxImages; }

        /**
         * Get the number of currently registered images.
         */
        uint32_t GetImageCount() const { return m_ImageCount; }

        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }

      private:
        std::vector<VkDescriptorSetLayoutBinding> GetDescriptorSetLayoutBindings() const;
        void CreateDescriptorSetLayout();
        void AllocateDescriptorSet();
        void InitImagesStructures();

        // Writes pending sampled-image descriptors while the registry mutex is held.
        void UpdateImageDescriptors(const std::vector<Ref<Image>>& images) const;

        SResourceHandle AllocateImageHandle();
        void FreeImageHandle(SResourceHandle handle);

        uint32_t GetMaxDescriptorsByType(VkDescriptorType type) const;

        uint32_t m_MaxImages;
        uint32_t m_MaxSamplers;

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

        struct SImageEntry
        {
            Ref<Image> Resource;
            uint32_t RefCount = 0;
        };

        std::vector<SImageEntry> m_Images;
        std::vector<Ref<Image>> m_DirtyImages;
        std::unordered_map<Image*, SResourceHandle> m_ImageLookup;
        uint32_t m_NextFreeIndex = 0;
        std::vector<uint32_t> m_RecycledIndices;
        uint32_t m_ImageCount = 0;
        mutable std::mutex m_ImageMutex;
    };
}
