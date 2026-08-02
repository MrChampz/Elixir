#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Material/MaterialRenderScene.h>
#include <Engine/Material/MaterialFrameTable.h>
#include <Engine/Material/MaterialRenderer.h>
#include <Engine/Material/MaterialTextureRegistry.h>

namespace Elixir
{
    struct SMaterialFrameSnapshot
    {
        Ref<const MaterialFrameTable> Table;
        uint32_t MaterialCount = 0;
        uint64_t SubmissionSerial = 0;
    };

    class ELIXIR_API MaterialSystem final
    {
    public:
        MaterialSystem(const GraphicsContext* context, uint32_t capacity);

        SMaterialFrameSnapshot BuildFrameSnapshot(
            const MaterialRenderScene& scene,
            uint64_t submissionSerial
        );

        std::optional<SMaterialProgramKey> GetProgramKey(
            EMaterialPass pass,
            const MaterialRenderProxy& material
        ) const;

        std::optional<SPreparedMaterialPass> PrepareMaterialPass(
            const SMaterialPassRequest& request
        ) const;

        template <typename T>
        requires std::invocable<T, const Ref<CommandBuffer>&, const Ref<Shader>&>
        void DrawMaterial(const SMaterialDrawRequest& request, T&& recordGeometry) const
        {
            m_Renderer->Draw(request, std::forward<T>(recordGeometry));
        }

        const Ref<DynamicStorageBuffer>& GetFrameBuffer() const { return m_FrameBuffer; }
        const Ref<TextureSet>& GetTextureSet() const { return m_Textures.GetTextureSet(); }
        const Ref<Sampler>& GetSampler() const { return m_Textures.GetSampler(); }
        uint32_t GetFallbackTextureIndex() const { return m_Textures.GetFallbackIndex(); }
        uint32_t FindTextureIndex(const Ref<Texture>& texture) const;

    private:
        uint32_t m_MaterialCapacity = 0;
        Ref<DynamicStorageBuffer> m_FrameBuffer;
        MaterialTextureRegistry m_Textures;
        Scope<MaterialRenderer> m_Renderer;
    };
}