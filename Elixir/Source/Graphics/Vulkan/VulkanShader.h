#pragma once

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

        void SetConstantBuffer(const std::string& name, void* data, size_t size) override;

        void BindTexture(const std::string& name, const Ref<Texture>& texture) override;
        void BindTextureSet(const std::string& name, const Ref<TextureSet>& set) override;
        void BindSampler(const std::string& name, const Ref<Sampler>& sampler) override;
        void BindStorageBuffer(const std::string& name, const Ref<StorageBuffer>& buffer) override;
        void BindStorageBuffer(const std::string& name, const Ref<DynamicStorageBuffer>& buffer) override;
        void BindConstantBuffer(const std::string& name, const Ref<UniformBuffer>& buffer) override;

        const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
        std::vector<VkDescriptorSet> GetDescriptorSets() const;
        const VkPipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }
        std::vector<VkPushConstantRange> GetPushConstantRanges() const;

      protected:
        void CreateDescriptorSetLayouts();
        void CreateDescriptorSets();
        void CreatePipelineLayout();

        void UpdateDescriptorSets();

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding, const Texture* texture
        ) const;
        void UpdateDescriptorSet(SShaderBinding binding, const Texture* texture) const;

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<Sampler>& sampler
        ) const;
        void UpdateDescriptorSet(SShaderBinding binding, const Ref<Sampler>& sampler) const;

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<StorageBuffer>& buffer
        ) const;
        void UpdateDescriptorSet(
            SShaderBinding binding,
            const Ref<StorageBuffer>& buffer
        ) const;

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<DynamicStorageBuffer>& buffer
        ) const;
        void UpdateDescriptorSet(
            SShaderBinding binding,
            const Ref<DynamicStorageBuffer>& buffer
        ) const;

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<UniformBuffer>& buffer
        ) const;
        void UpdateDescriptorSet(
            SShaderBinding binding,
            const Ref<UniformBuffer>& buffer
        ) const;

        bool m_BindlessSet = false;

        std::vector<VkDescriptorSet> m_DescriptorSets;
        std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;

        VkPipelineLayout m_PipelineLayout;

        mutable std::unordered_map<SShaderBinding, std::vector<VkDescriptorImageInfo>> m_ImageInfoCache;
        mutable std::unordered_map<SShaderBinding, VkDescriptorBufferInfo> m_BufferInfoCache;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
