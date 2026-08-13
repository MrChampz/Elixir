#pragma once

#include <Engine.h>
#include <RmlUi/Core/Matrix4.h>
#include <RmlUi/Core/RenderInterface.h>

class RmlUiRenderer final : public Rml::RenderInterface
{
public:
    RmlUiRenderer(const Elixir::GraphicsContext* context, const Elixir::ShaderLoader* shaderLoader);

    Rml::TextureHandle RegisterExternalTexture(const Ref<Elixir::Texture2D>& texture);
    void UpdateExternalTexture(
        Rml::TextureHandle textureHandle,
        const Ref<Elixir::Texture2D>& texture
    );
    void BeginFrame(const Elixir::Extent2D& extent);
    void RenderTexture(
        Rml::TextureHandle texture,
        const glm::vec2& origin,
        const glm::vec2& size
    );
    void EndFrame();

    void RenderGeometry(
        Rml::Vertex* vertices,
        int numVertices,
        int* indices,
        int numIndices,
        Rml::TextureHandle texture,
        const Rml::Vector2f& translation
    ) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(int x, int y, int width, int height) override;
    void SetTransform(const Rml::Matrix4f* transform) override;
    bool GenerateTexture(
        Rml::TextureHandle& textureHandle,
        const Rml::byte* source,
        const Rml::Vector2i& sourceDimensions
    ) override;
    bool LoadTexture(
        Rml::TextureHandle& textureHandle,
        Rml::Vector2i& textureDimensions,
        const Rml::String& source
    ) override;
    void ReleaseTexture(Rml::TextureHandle textureHandle) override;

private:
    static constexpr size_t MaxVertices = 16 * 1024;
    static constexpr size_t MaxIndices = 48 * 1024;

    struct SVertex
    {
        glm::vec2 Position;
        glm::vec4 Color;
        glm::vec2 TexCoord;
        uint32_t TextureIndex;
    };

    struct SDraw
    {
        uint32_t FirstIndex;
        uint32_t IndexCount;
        Elixir::Rect2D Scissor;
    };

    struct SFrameData
    {
        glm::vec2 Viewport;
    };

    struct STexture
    {
        Ref<Elixir::Texture2D> Texture;
        Elixir::SResourceHandle Resource;
    };

    Elixir::Rect2D GetActiveScissor() const;

    const Elixir::GraphicsContext* m_GraphicsContext;
    Elixir::Extent2D m_Extent{};
    bool m_ScissorEnabled = false;
    bool m_TransformEnabled = false;
    bool m_ReportedFirstFrame = false;
    Elixir::Rect2D m_Scissor{};
    Rml::Matrix4f m_Transform = Rml::Matrix4f::Identity();

    Ref<Elixir::Shader> m_Shader;
    Ref<Elixir::GraphicsPipeline> m_Pipeline;
    Ref<Elixir::DynamicVertexBuffer> m_VertexBuffer;
    Ref<Elixir::DynamicIndexBuffer> m_IndexBuffer;
    Ref<Elixir::UniformBuffer> m_FrameConstantBuffer;
    Ref<Elixir::TextureSet> m_TextureSet;
    Ref<Elixir::Texture2D> m_WhiteTexture;
    Elixir::SResourceHandle m_WhiteTextureResource;

    std::vector<SVertex> m_Vertices;
    std::vector<uint32_t> m_Indices;
    std::vector<SDraw> m_Draws;
    std::unordered_map<Rml::TextureHandle, STexture> m_Textures;
    Rml::TextureHandle m_NextTextureHandle = 1;
};
