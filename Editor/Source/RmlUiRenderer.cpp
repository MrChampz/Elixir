#include "RmlUiRenderer.h"

#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Graphics/TextureLoader.h>

using namespace Elixir;

RmlUiRenderer::RmlUiRenderer(const GraphicsContext* context, const ShaderLoader* shaderLoader)
    : m_GraphicsContext(context)
{
    const BufferLayout layout({
        {
            {
                { EDataType::Vec2, "Position" },
                { EDataType::Vec4, "Color" },
                { EDataType::Vec2, "TexCoord" },
                { EDataType::UInt, "TextureIndex" },
            },
            EInputRate::Vertex,
        },
    });

    m_Shader = shaderLoader->LoadShader("./Shaders/", "RmlUi");
    m_FrameConstantBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(SFrameData));
    m_Shader->BindConstantBuffer("cbRmlUiFrame", m_FrameConstantBuffer);

    constexpr uint32_t WhitePixel = 0xffffffff;
    m_WhiteTexture = Texture2D::Create(
        m_GraphicsContext,
        EImageFormat::R8G8B8A8_UNORM,
        1,
        1,
        &WhitePixel,
        "RmlUiWhite"
    );
    m_TextureSet = TextureSet::Create(m_GraphicsContext);
    m_WhiteTextureResource = m_TextureSet->AddTexture(m_WhiteTexture);
    m_Shader->BindTextureSet("textures", m_TextureSet);

    const auto sampler = SamplerBuilder()
        .SetMagFilter(ESamplerFilter::Linear)
        .SetMinFilter(ESamplerFilter::Linear)
        .SetAddressModeU(ESamplerAddressMode::ClampToEdge)
        .SetAddressModeV(ESamplerAddressMode::ClampToEdge)
        .Build(m_GraphicsContext);
    m_Shader->BindSampler("textureSampler", sampler);

    PipelineBuilder builder;
    builder.SetShader(m_Shader);
    builder.SetInputTopology(EPrimitiveTopology::TriangleList);
    builder.SetPolygonMode(EPolygonMode::Fill);
    builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
    builder.EnableAlphaBlending();
    builder.DisableDepthTest();
    builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
    builder.SetBufferLayout(layout);
    m_Pipeline = builder.Build(m_GraphicsContext);

    m_VertexBuffer = DynamicVertexBuffer::Create(m_GraphicsContext, MaxVertices * sizeof(SVertex));
    m_VertexBuffer->SetLayout(layout);
    m_IndexBuffer = DynamicIndexBuffer::Create(m_GraphicsContext, MaxIndices * sizeof(uint32_t));
}

Rml::TextureHandle RmlUiRenderer::RegisterExternalTexture(const Ref<Texture2D>& texture)
{
    EE_CORE_ASSERT(texture, "Cannot register a null RmlUi texture.")

    const Rml::TextureHandle textureHandle = m_NextTextureHandle++;
    m_Textures.emplace(textureHandle, STexture {
        .Texture = texture,
        .Resource = m_TextureSet->AddTexture(texture),
    });
    return textureHandle;
}

void RmlUiRenderer::UpdateExternalTexture(
    const Rml::TextureHandle textureHandle,
    const Ref<Texture2D>& texture
)
{
    EE_CORE_ASSERT(texture, "Cannot update an RmlUi texture with a null texture.")

    const auto textureIt = m_Textures.find(textureHandle);
    EE_CORE_ASSERT(textureIt != m_Textures.end(), "Cannot update an unknown RmlUi texture.")

    m_TextureSet->RemoveTexture(textureIt->second.Resource);
    textureIt->second.Resource = m_TextureSet->AddTexture(texture);
    m_TextureSet->FlushChanges();
    textureIt->second.Texture = texture;
}

void RmlUiRenderer::BeginFrame(const Extent2D& extent)
{
    m_Extent = extent;
    m_ScissorEnabled = false;
    m_TransformEnabled = false;
    m_Transform = Rml::Matrix4f::Identity();
    m_Vertices.clear();
    m_Indices.clear();
    m_Draws.clear();
}

void RmlUiRenderer::RenderTexture(
    const Rml::TextureHandle texture,
    const glm::vec2& origin,
    const glm::vec2& size
)
{
    if (size.x <= 0.0f || size.y <= 0.0f)
        return;

    const auto textureIt = m_Textures.find(texture);
    if (textureIt == m_Textures.end())
    {
        EE_CORE_ERROR("Cannot render an unknown external texture: {}.", texture)
        return;
    }

    const uint32_t vertexOffset = static_cast<uint32_t>(m_Vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(m_Indices.size());
    const uint32_t textureIndex = textureIt->second.Resource.Index;

    m_Vertices.insert(m_Vertices.end(), {
        {
            .Position = origin,
            .Color = { 1.0f, 1.0f, 1.0f, 1.0f },
            .TexCoord = { 0.0f, 0.0f },
            .TextureIndex = textureIndex,
        },
        {
            .Position = origin + glm::vec2(size.x, 0.0f),
            .Color = { 1.0f, 1.0f, 1.0f, 1.0f },
            .TexCoord = { 1.0f, 0.0f },
            .TextureIndex = textureIndex,
        },
        {
            .Position = origin + size,
            .Color = { 1.0f, 1.0f, 1.0f, 1.0f },
            .TexCoord = { 1.0f, 1.0f },
            .TextureIndex = textureIndex,
        },
        {
            .Position = origin + glm::vec2(0.0f, size.y),
            .Color = { 1.0f, 1.0f, 1.0f, 1.0f },
            .TexCoord = { 0.0f, 1.0f },
            .TextureIndex = textureIndex,
        },
    });
    m_Indices.insert(m_Indices.end(), {
        vertexOffset,
        vertexOffset + 1,
        vertexOffset + 2,
        vertexOffset,
        vertexOffset + 2,
        vertexOffset + 3,
    });
    m_Draws.push_back({
        .FirstIndex = firstIndex,
        .IndexCount = 6,
        .Scissor = {
            .Offset = {
                static_cast<int32_t>(std::max(origin.x, 0.0f)),
                static_cast<int32_t>(std::max(origin.y, 0.0f)),
            },
            .Extent = {
                static_cast<uint32_t>(std::max(size.x, 0.0f)),
                static_cast<uint32_t>(std::max(size.y, 0.0f)),
            },
        },
    });
}

void RmlUiRenderer::EndFrame()
{
    if (m_Draws.empty())
        return;

    EE_CORE_ASSERT(m_Vertices.size() <= MaxVertices, "RmlUi vertex buffer capacity exceeded.")
    EE_CORE_ASSERT(m_Indices.size() <= MaxIndices, "RmlUi index buffer capacity exceeded.")

    if (!m_ReportedFirstFrame)
    {
        EE_CORE_INFO(
            "RmlUi rendered its first frame: {} draws, {} vertices, {} indices.",
            m_Draws.size(),
            m_Vertices.size(),
            m_Indices.size()
        )
        m_ReportedFirstFrame = true;
    }

    const SFrameData frameData = { .Viewport = {
        static_cast<float>(m_Extent.Width),
        static_cast<float>(m_Extent.Height),
    } };
    m_FrameConstantBuffer->UpdateData(&frameData, sizeof(frameData));
    m_VertexBuffer->UpdateData(m_Vertices.data(), m_Vertices.size() * sizeof(SVertex));
    m_IndexBuffer->UpdateData(m_Indices.data(), m_Indices.size() * sizeof(uint32_t));

    const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
    cmd->BeginRendering({
        .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
        .RenderArea = m_Extent,
    });
    cmd->SetViewports({ {
        .X = 0.0f,
        .Y = 0.0f,
        .Width = static_cast<float>(m_Extent.Width),
        .Height = static_cast<float>(m_Extent.Height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    } });

    m_Pipeline->Bind(cmd);
    m_VertexBuffer->Bind(cmd);
    m_IndexBuffer->Bind(cmd);

    for (const auto& draw : m_Draws)
    {
        cmd->SetScissors({ draw.Scissor });
        cmd->DrawIndexed(draw.IndexCount, 1, draw.FirstIndex);
    }

    cmd->EndRendering();
    m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
}

void RmlUiRenderer::RenderGeometry(
    Rml::Vertex* vertices,
    const int numVertices,
    int* indices,
    const int numIndices,
    const Rml::TextureHandle texture,
    const Rml::Vector2f& translation
)
{
    uint32_t textureIndex = m_WhiteTextureResource.Index;
    if (texture != 0)
    {
        const auto textureIt = m_Textures.find(texture);
        if (textureIt == m_Textures.end())
        {
            EE_CORE_ERROR("RmlUi submitted an unknown texture handle: {}.", texture)
            return;
        }
        textureIndex = textureIt->second.Resource.Index;
    }

    const auto vertexOffset = static_cast<uint32_t>(m_Vertices.size());
    m_Vertices.reserve(m_Vertices.size() + numVertices);
    for (int index = 0; index < numVertices; ++index)
    {
        const auto& vertex = vertices[index];
        Rml::Vector4f position {
            vertex.position.x + translation.x,
            vertex.position.y + translation.y,
            0.0f,
            1.0f
        };
        if (m_TransformEnabled)
        {
            position = m_Transform * position;
            if (position.w != 0.0f && position.w != 1.0f)
            {
                position.x /= position.w;
                position.y /= position.w;
            }
        }

        m_Vertices.push_back({
            .Position = { position.x, position.y },
            .Color = {
                vertex.colour.red / 255.0f,
                vertex.colour.green / 255.0f,
                vertex.colour.blue / 255.0f,
                vertex.colour.alpha / 255.0f,
            },
            .TexCoord = { vertex.tex_coord.x, vertex.tex_coord.y },
            .TextureIndex = textureIndex,
        });
    }

    const auto firstIndex = static_cast<uint32_t>(m_Indices.size());
    m_Indices.reserve(m_Indices.size() + numIndices);
    for (int index = 0; index < numIndices; ++index)
        m_Indices.push_back(vertexOffset + static_cast<uint32_t>(indices[index]));

    m_Draws.push_back({
        .FirstIndex = firstIndex,
        .IndexCount = static_cast<uint32_t>(numIndices),
        .Scissor = GetActiveScissor(),
    });
}

bool RmlUiRenderer::GenerateTexture(
    Rml::TextureHandle& textureHandle,
    const Rml::byte* source,
    const Rml::Vector2i& sourceDimensions
)
{
    if (!source || sourceDimensions.x <= 0 || sourceDimensions.y <= 0)
        return false;

    const auto texture = Texture2D::Create(
        m_GraphicsContext,
        EImageFormat::R8G8B8A8_UNORM,
        static_cast<uint32_t>(sourceDimensions.x),
        static_cast<uint32_t>(sourceDimensions.y),
        source,
        "RmlUiGenerated"
    );

    if (!texture)
        return false;

    textureHandle = RegisterExternalTexture(texture);
    return true;
}

bool RmlUiRenderer::LoadTexture(
    Rml::TextureHandle& textureHandle,
    Rml::Vector2i& textureDimensions,
    const Rml::String& source
)
{
    const auto texture = std::static_pointer_cast<Texture2D>(
        TextureLoader::Load(source, EImageFormat::R8G8B8A8_UNORM)
    );
    if (!texture)
        return false;

    textureDimensions = {
        static_cast<int>(texture->GetWidth()),
        static_cast<int>(texture->GetHeight()),
    };
    textureHandle = RegisterExternalTexture(texture);
    return true;
}

void RmlUiRenderer::ReleaseTexture(const Rml::TextureHandle textureHandle)
{
    if (const auto textureIt = m_Textures.find(textureHandle); textureIt != m_Textures.end())
    {
        m_TextureSet->RemoveTexture(textureIt->second.Resource);
        m_Textures.erase(textureIt);
    }
}

void RmlUiRenderer::EnableScissorRegion(const bool enable)
{
    m_ScissorEnabled = enable;
}

void RmlUiRenderer::SetScissorRegion(const int x, const int y, const int width, const int height)
{
    m_Scissor = {
        .Offset = { x, y },
        .Extent = { static_cast<uint32_t>(std::max(width, 0)), static_cast<uint32_t>(std::max(height, 0)) },
    };
}

void RmlUiRenderer::SetTransform(const Rml::Matrix4f* transform)
{
    m_TransformEnabled = transform != nullptr;
    m_Transform = transform ? *transform : Rml::Matrix4f::Identity();
}

Rect2D RmlUiRenderer::GetActiveScissor() const
{
    if (m_ScissorEnabled)
        return m_Scissor;

    return {
        .Offset = { 0, 0 },
        .Extent = m_Extent,
    };
}
