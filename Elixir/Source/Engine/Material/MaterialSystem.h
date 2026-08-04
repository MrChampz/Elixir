#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Material/MaterialRenderScene.h>
#include <Engine/Material/MaterialFrameTable.h>
#include <Engine/Material/MaterialRenderer.h>
#include <Engine/Material/MaterialTextureRegistry.h>

namespace Elixir
{
    struct SMaterialSystemConfig
    {
        uint32_t InitialFrameCapacity = 256;
    };

    struct SMaterialFrameSnapshot
    {
        Ref<const MaterialFrameTable> Table;
        uint32_t MaterialCount = 0;
        uint64_t SubmissionSerial = 0;
    };

    struct SMaterialRenderResult
    {
        uint32_t BatchCount = 0;
        uint32_t DrawCount = 0;
    };

    class ELIXIR_API MaterialSystem final
    {
    public:
        MaterialSystem(
            const GraphicsContext* context,
            SMaterialSystemConfig config
        );

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

        SMaterialRenderResult Render(
            const Ref<CommandBuffer>& cmd,
            const MaterialRenderScene& scene,
            const SMaterialFrameSnapshot& snapshot
        ) const;

        const Ref<DynamicStorageBuffer>& GetFrameBuffer() const { return m_FrameBuffer; }
        const Ref<TextureSet>& GetTextureSet() const { return m_Textures.GetTextureSet(); }
        const Ref<Sampler>& GetSampler() const { return m_Textures.GetSampler(); }

    private:
        uint32_t m_MaterialCapacity = 0;
        Ref<DynamicStorageBuffer> m_FrameBuffer;
        MaterialTextureRegistry m_Textures;
        Scope<MaterialRenderer> m_Renderer;
    };
}