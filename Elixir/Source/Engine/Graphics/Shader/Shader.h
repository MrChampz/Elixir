#pragma once

#include <Engine/Graphics/Sampler.h>
#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/TextureSet.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Engine/Graphics/Shader/ShaderBinding.h>
#include <Engine/Graphics/Shader/ShaderModule.h>

namespace Elixir
{
    template <typename T, auto EnumValue>
    using EnumIndexedArray = std::array<T, static_cast<size_t>(EnumValue)>;

    template <typename T>
    using BindingTable = std::unordered_map<SShaderBinding, T>;

    struct SShaderResources
    {
        BindingTable<ShaderResource> Resources;
        BindingTable<ShaderStorageBuffer> StorageBuffers;
        BindingTable<ShaderConstantBuffer> ConstantBuffers;
        BindingTable<ShaderPushConstant> PushConstants;
    };

    struct SShaderModuleCreateInfo
    {
        std::filesystem::path Path;
        std::string Entrypoint;
        std::vector<ShaderResource> Resources;
        std::vector<ShaderStorageBuffer> StorageBuffers;
        std::vector<ShaderConstantBuffer> ConstantBuffers;
        std::vector<ShaderPushConstant> PushConstants;
        std::vector<Byte> Bytecode;

        SShaderModuleCreateInfo() = default;
        SShaderModuleCreateInfo(SShaderModuleCreateInfo&&) noexcept = default;
        SShaderModuleCreateInfo(const SShaderModuleCreateInfo&) = delete;

        SShaderModuleCreateInfo& operator=(SShaderModuleCreateInfo&&) noexcept = default;
        SShaderModuleCreateInfo& operator=(const SShaderModuleCreateInfo&) = delete;
    };

    struct SShaderCreateInfo
    {
        std::string Name;
        Scope<SShaderModuleCreateInfo> Vertex;
        Scope<SShaderModuleCreateInfo> Pixel;
        Scope<SShaderModuleCreateInfo> Compute;
    };

    class ELIXIR_API Shader
    {
      public:
        virtual ~Shader() = default;

        virtual void Bind(const Ref<CommandBuffer>& cmd, const Pipeline* pipeline) = 0;

        virtual void SetPushConstant(const std::string& name, void* data, size_t size) = 0;
        virtual void SetPushConstant(
            const Ref<CommandBuffer>& cmd,
            const std::string& name,
            void* data,
            size_t size
        ) = 0;

        /**
         * @brief Update a CPU-mappable constant buffer, or create one when none is bound.
         * @param name Shader constant resource binding name.
         * @param data Bytes to upload; must not be null.
         * @param size Number of bytes to upload, within the bound buffer capacity.
         */
        virtual void SetConstantBuffer(const std::string& name, void* data, size_t size) = 0;

        /**
         * @brief Binds an image resource for shader sampling.
         * @param name Name of the shader resource binding.
         * @param image Image to sample; it must remain valid while the shader uses it.
         */
        virtual void BindImage(const std::string& name, const Ref<Image>& image) = 0;

        virtual void BindTexture(const std::string& name, const Ref<Texture>& texture) = 0;
        virtual void BindTextureSet(const std::string& name, const Ref<TextureSet>& set) = 0;
        virtual void BindSampler(const std::string& name, const Ref<Sampler>& sampler) = 0;

        /**
         * @brief Bind a buffer that was created with storage usage.
         * @param name Shader storage resource binding name.
         * @param buffer Buffer with StorageBuffer usage, including a vertex buffer.
         */
        virtual void BindStorageBuffer(const std::string& name, const Ref<Buffer>& buffer) = 0;

        /**
         * @brief Bind a buffer that was created with uniform usage.
         * @param name Shader constant resource binding name.
         * @param buffer Buffer with UniformBuffer usage and sufficient capacity for the
         * resource.
         */
        virtual void BindConstantBuffer(const std::string& name, const Ref<Buffer>& buffer) = 0;

        /**
         * @brief Checks whether the shader declares a named binding.
         *
         * The shader compiler can remove declarations that are not used by an
         * optimized permutation.
         *
         * @param name Binding name to find.
         * @return True when the shader contains @p name binding.
         */
        bool HasBinding(const std::string& name) const;

        virtual Ref<Texture> GetTexture(const std::string& name) const;
        virtual Ref<Texture> GetTexture(SShaderBinding binding) const;

        virtual Ref<TextureSet> GetTextureSet(const std::string& name) const;
        virtual Ref<TextureSet> GetTextureSet(SShaderBinding binding) const;

        virtual Ref<Sampler> GetSampler(const std::string& name) const;
        virtual Ref<Sampler> GetSampler(SShaderBinding binding) const;

        const std::string& GetName() const { return m_Name; }
        Ref<ShaderModule> GetModule(EShaderStage stage) const;

        auto GetModules() const
        {
            return m_Modules | std::views::filter([] (const Ref<ShaderModule>& module)
            {
                return module != nullptr;
            });
        }

        static Ref<Shader> Create(
            const GraphicsContext* context,
            SShaderCreateInfo&& info
        );

      protected:
        explicit Shader(const GraphicsContext* context, SShaderCreateInfo&& info);

        Ref<ShaderModule> CreateModule(
            EShaderStage stage,
            const Scope<SShaderModuleCreateInfo>& info
        );

        ShaderResource* AddResourceToBindingTable(ShaderResource resource);
        ShaderStorageBuffer* AddStorageBufferToBindingTable(ShaderStorageBuffer buffer);
        ShaderConstantBuffer* AddConstantBufferToBindingTable(ShaderConstantBuffer buffer);
        ShaderPushConstant* AddPushConstantToBindingTable(ShaderPushConstant constant);

        void CreateBindingLookup();

        void SaveShaderBindingToLookup(const std::string& name, SShaderBinding binding);
        const SShaderBinding* GetShaderBinding(const std::string& name) const;

        std::string m_Name;
        EnumIndexedArray<Ref<ShaderModule>, EShaderStage::Count> m_Modules;

        // Shader resources declarations
        SShaderResources m_Resources;

        // Shader resources
        std::unordered_map<std::string, SShaderBinding> m_BindingLookup;
        std::unordered_map<SShaderBinding, Ref<Image>> m_Images;
        std::unordered_map<SShaderBinding, Ref<Texture>> m_Textures;
        std::unordered_map<SShaderBinding, Ref<TextureSet>> m_TextureSets;
        std::unordered_map<SShaderBinding, Ref<Sampler>> m_Samplers;
        std::unordered_map<SShaderBinding, Ref<Buffer>> m_StorageBuffers;
        // TODO: Rename to ConstantBuffer
        std::unordered_map<SShaderBinding, Ref<Buffer>> m_ConstantBuffers;
        std::unordered_map<SShaderBinding, Ref<PushConstantBuffer>> m_PushConstants;

        const GraphicsContext* m_GraphicsContext;
    };
}
