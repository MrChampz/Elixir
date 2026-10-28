#pragma once

#include <Engine/Graphics/FrameSlotPendingState.h>
#include <Engine/Graphics/Shader/Shader.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class VulkanTextureSet;

    class ELIXIR_API VulkanShader final : public Shader
    {
        friend class VulkanTextureSet;
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

        /**
         * @brief Update a mappable constant buffer, or create one when none is bound.
         * @param name Shader constant resource binding name.
         * @param data Bytes to upload; must not be null.
         * @param size Number of bytes to upload, within the bound buffer capacity.
         */
        void SetConstantBuffer(const std::string& name, void* data, size_t size) override;

        void BindImage(const std::string& name, const Ref<Image>& image) override;
        void BindTexture(const std::string& name, const Ref<Texture>& texture) override;
        void BindTextureSet(const std::string& name, const Ref<TextureSet>& set) override;
        void BindSampler(const std::string& name, const Ref<Sampler>& sampler) override;

        /**
         * @brief Bind a buffer after checking storage usage and device limits.
         * @param name Shader storage resource binding name.
         * @param buffer Buffer with StorageBuffer usage, including a vertex buffer.
         */
        void BindStorageBuffer(const std::string& name, const Ref<Buffer>& buffer) override;


        /**
         * @brief Bind a buffer after checking uniform usage, capacity, and device limits.
         * @param name Shader constant resource binding name.
         * @param buffer Buffer with UniformBuffer usage and sufficient capacity for the
         * resource.
         */
        void BindConstantBuffer(const std::string& name, const Ref<Buffer>& buffer) override;

        const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
        std::vector<VkDescriptorSet> GetDescriptorSets() const;
        const VkPipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }
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

        /**
         * @brief Build a buffer descriptor using the shader resource type.
         * @param binding Uniform or storage resource binding declared by the shader.
         * @param buffer Buffer with validated usage and a valid byte range.
         */
        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<Buffer>& buffer
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
            Ref<Buffer>
        >;

        using DescriptorSetState = FrameSlotPendingState<
            std::vector<VkDescriptorSet>,
            SShaderBinding,
            DescriptorValue
        >;

        DescriptorSetState m_DescriptorSets;
        std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
        std::optional<uint32_t> m_BindlessSetIndex;

        VkPipelineLayout m_PipelineLayout;

        mutable std::unordered_map<SShaderBinding, std::vector<VkDescriptorImageInfo>> m_ImageInfoCache;
        mutable std::unordered_map<SShaderBinding, VkDescriptorBufferInfo> m_BufferInfoCache;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
