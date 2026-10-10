#pragma once

#include <Engine/Graphics/FrameSlotPendingState.h>
#include <Engine/Graphics/Shader/Shader.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class VulkanImageSet;

    class ELIXIR_API VulkanShader final : public Shader
    {
        friend class VulkanImageSet;
      public:
        VulkanShader(const GraphicsContext* context, SShaderCreateInfo&& info);
        ~VulkanShader() override;

        void Bind(const Ref<CommandBuffer>& cmd, const Pipeline* pipeline) override;

        void SetPushConstant(const std::string& name, void* data, size_t size) override;
        void SetPushConstant(
            const Ref<CommandBuffer>& cmd,
            const std::string& name,
            void* data,
            size_t size
        ) override;

        void SetConstantBuffer(const std::string& name, void* data, size_t size) override;

        void BindImage(const std::string& name, const Ref<Image>& image) override;
        void BindTexture(const std::string& name, const Ref<Texture>& texture) override;

        /** @brief Binds an image set to a named shader resource. */
        void BindImageSet(const std::string& name, const Ref<ImageSet>& set) override;

        void BindSampler(const std::string& name, const Ref<Sampler>& sampler) override;
        void BindStorageBuffer(const std::string& name, const Ref<StorageBuffer>& buffer) override;
        void BindStorageBuffer(const std::string& name, const Ref<DynamicStorageBuffer>& buffer) override;
        void BindConstantBuffer(const std::string& name, const Ref<UniformBuffer>& buffer) override;

        const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
        std::vector<VkDescriptorSet> GetDescriptorSets() const;
        const VkPipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }

        /**
         * @brief Returns whether descriptor layouts, descriptor sets, and the pipeline
         * layout were created.
         */
        bool IsValid() const { return m_Valid; }

        std::vector<VkPushConstantRange> GetPushConstantRanges() const;

      protected:
        void CreateDescriptorSetLayouts();
        void CreateDescriptorSets();
        void CreatePipelineLayout();

        void ApplyPendingDescriptorState();
        void RefreshImageDescriptorState();

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding, const Image* image
        ) const;
        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding, const Texture* texture
        ) const;
        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<Sampler>& sampler
        ) const;
        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<StorageBuffer>& buffer
        ) const;
        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<DynamicStorageBuffer>& buffer
        ) const;
        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<UniformBuffer>& buffer
        ) const;

        struct SImageDescriptorValue
        {
            Ref<Image> Image;
            uint64_t ResourceGeneration = 0;
            VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            bool operator==(const SImageDescriptorValue&) const = default;
        };

        struct STextureDescriptorValue
        {
            Ref<Texture> Texture;
            uint64_t ResourceGeneration = 0;
            VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            bool operator==(const STextureDescriptorValue&) const = default;
        };

        using DescriptorValue = std::variant<
            SImageDescriptorValue,
            STextureDescriptorValue,
            Ref<Sampler>,
            Ref<StorageBuffer>,
            Ref<DynamicStorageBuffer>,
            Ref<UniformBuffer>
        >;

        using DescriptorSetState = FrameSlotPendingState<
            std::vector<VkDescriptorSet>,
            SShaderBinding,
            DescriptorValue
        >;

        bool m_Valid = true;

        DescriptorSetState m_DescriptorSets;
        std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
        std::optional<uint32_t> m_BindlessSetIndex;

        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

        mutable std::unordered_map<SShaderBinding, std::vector<VkDescriptorImageInfo>> m_ImageInfoCache;
        mutable std::unordered_map<SShaderBinding, VkDescriptorBufferInfo> m_BufferInfoCache;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
