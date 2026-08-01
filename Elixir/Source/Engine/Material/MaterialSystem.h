#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Material/MaterialFrameTable.h>
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
            std::span<const Ref<const MaterialRenderProxy>> materials,
            std::span<const Ref<Texture>> textures,
            uint64_t submissionSerial
        );

        const Ref<DynamicStorageBuffer>& GetFrameBuffer() const { return m_FrameBuffer; }
        const Ref<TextureSet>& GetTextureSet() const { return m_Textures.GetTextureSet(); }
        const Ref<Sampler>& GetSampler() const { return m_Textures.GetSampler(); }
        uint32_t GetFallbackTextureIndex() const { return m_Textures.GetFallbackIndex(); }
        uint32_t FindTextureIndex(const Ref<Texture>& texture) const;

    private:
        uint32_t m_MaterialCapacity = 0;
        Ref<DynamicStorageBuffer> m_FrameBuffer;
        MaterialTextureRegistry m_Textures;
    };
}