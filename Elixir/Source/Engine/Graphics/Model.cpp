#include "epch.h"
#include "Model.h"

#include "Engine/Graphics/TextureLoader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <stb_image.h>

namespace Elixir
{
    static glm::mat4 ToGlm(const fastgltf::math::fmat4x4& m)
    {
        // fastgltf math is column-major, matching glm.
        glm::mat4 r(1.0f);
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
                r[c][row] = m[c][row];
        return r;
    }

    // Decode compressed image bytes (png/jpg) into an RGBA sRGB texture.
    static Ref<Texture> DecodeImage(const GraphicsContext* context, const std::byte* bytes, size_t size)
    {
        int w = 0, h = 0, channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes), (int)size, &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            EE_CORE_WARN("Failed to decode embedded glTF image: {0}", stbi_failure_reason());
            return nullptr;
        }

        auto texture = Texture2D::Create(
            context, EImageFormat::R8G8B8A8_SRGB, (uint32_t)w, (uint32_t)h, pixels);
        stbi_image_free(pixels);
        return texture;
    }

    // Resolve a glTF image (external URI or embedded buffer/bufferView) to a texture.
    static Ref<Texture> LoadImage(
        const GraphicsContext* context,
        fastgltf::Asset& asset,
        size_t imageIndex,
        const std::filesystem::path& dir)
    {
        Ref<Texture> result;
        auto& image = asset.images[imageIndex];

        std::visit(fastgltf::visitor{
            [](auto&) {},
            [&](fastgltf::sources::URI& uri)
            {
                const auto imagePath = dir / uri.uri.fspath();
                if (std::filesystem::exists(imagePath))
                    result = TextureLoader::Load(imagePath);
                else
                    EE_CORE_WARN("glTF texture not found: {0}", imagePath.string());
            },
            [&](fastgltf::sources::Array& arr)
            {
                result = DecodeImage(context, arr.bytes.data(), arr.bytes.size());
            },
            [&](fastgltf::sources::Vector& vec)
            {
                result = DecodeImage(context, vec.bytes.data(), vec.bytes.size());
            },
            [&](fastgltf::sources::BufferView& view)
            {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];
                std::visit(fastgltf::visitor{
                    [](auto&) {},
                    [&](fastgltf::sources::Array& arr)
                    {
                        result = DecodeImage(context, arr.bytes.data() + bufferView.byteOffset, bufferView.byteLength);
                    },
                    [&](fastgltf::sources::Vector& vec)
                    {
                        result = DecodeImage(context, vec.bytes.data() + bufferView.byteOffset, bufferView.byteLength);
                    },
                }, buffer.data);
            },
        }, image.data);

        return result;
    }

    Ref<Model> Model::Load(const GraphicsContext* context, const std::filesystem::path& path)
    {
        auto dataResult = fastgltf::GltfDataBuffer::FromPath(path);
        if (dataResult.error() != fastgltf::Error::None)
        {
            EE_CORE_ERROR("Failed to open glTF file: {0}", path.string());
            return nullptr;
        }

        const auto dir = path.parent_path();

        fastgltf::Parser parser;
        constexpr auto options =
            fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadGLBBuffers;

        auto assetResult = parser.loadGltf(dataResult.get(), dir, options);
        if (assetResult.error() != fastgltf::Error::None)
        {
            EE_CORE_ERROR("Failed to parse glTF file: {0} ({1})",
                path.string(), fastgltf::getErrorMessage(assetResult.error()));
            return nullptr;
        }

        fastgltf::Asset& asset = assetResult.get();
        auto model = CreateRef<Model>();

        // Cache textures by glTF texture index so shared materials load once.
        std::unordered_map<size_t, Ref<Texture>> textureCache;
        auto resolveTexture = [&](size_t textureIndex) -> Ref<Texture>
        {
            if (const auto it = textureCache.find(textureIndex); it != textureCache.end())
                return it->second;

            Ref<Texture> texture;
            const auto& gltfTexture = asset.textures[textureIndex];
            if (gltfTexture.imageIndex)
                texture = LoadImage(context, asset, *gltfTexture.imageIndex, dir);

            textureCache[textureIndex] = texture;
            return texture;
        };

        const size_t sceneIndex = asset.defaultScene.value_or(0);
        fastgltf::iterateSceneNodes(asset, sceneIndex, fastgltf::math::fmat4x4(),
            [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& worldMatrix)
            {
                if (!node.meshIndex)
                    return;

                const glm::mat4 transform = ToGlm(worldMatrix);
                auto& mesh = asset.meshes[*node.meshIndex];

                for (auto& primitive : mesh.primitives)
                {
                    const auto* posAttr = primitive.findAttribute("POSITION");
                    if (posAttr == primitive.attributes.end())
                        continue;

                    auto& posAccessor = asset.accessors[posAttr->accessorIndex];
                    std::vector<SModelVertex> vertices(posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, posAccessor, [&](fastgltf::math::fvec3 v, size_t i)
                        {
                            vertices[i].Position = { v.x(), v.y(), v.z() };
                            vertices[i].Normal = { 0.0f, 1.0f, 0.0f };
                            vertices[i].TexCoord = { 0.0f, 0.0f };
                        });

                    if (const auto* normAttr = primitive.findAttribute("NORMAL");
                        normAttr != primitive.attributes.end())
                    {
                        auto& accessor = asset.accessors[normAttr->accessorIndex];
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                            asset, accessor, [&](fastgltf::math::fvec3 v, size_t i)
                            {
                                vertices[i].Normal = { v.x(), v.y(), v.z() };
                            });
                    }

                    if (const auto* uvAttr = primitive.findAttribute("TEXCOORD_0");
                        uvAttr != primitive.attributes.end())
                    {
                        auto& accessor = asset.accessors[uvAttr->accessorIndex];
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                            asset, accessor, [&](fastgltf::math::fvec2 v, size_t i)
                            {
                                vertices[i].TexCoord = { v.x(), v.y() };
                            });
                    }

                    std::vector<uint32_t> indices;
                    if (primitive.indicesAccessor)
                    {
                        auto& accessor = asset.accessors[*primitive.indicesAccessor];
                        indices.resize(accessor.count);
                        fastgltf::iterateAccessorWithIndex<uint32_t>(
                            asset, accessor, [&](uint32_t v, size_t i) { indices[i] = v; });
                    }
                    else
                    {
                        indices.resize(vertices.size());
                        for (uint32_t i = 0; i < indices.size(); ++i)
                            indices[i] = i;
                    }

                    if (vertices.empty() || indices.empty())
                        continue;

                    SModelPrimitive prim;
                    prim.Transform = transform;
                    prim.IndexCount = (uint32_t)indices.size();
                    prim.Vertices = VertexBuffer::Create(
                        context, vertices.size() * sizeof(SModelVertex), vertices.data());
                    prim.Indices = IndexBuffer::Create(
                        context, indices.size() * sizeof(uint32_t), indices.data(), EIndexType::UInt32);

                    if (primitive.materialIndex)
                    {
                        auto& material = asset.materials[*primitive.materialIndex];
                        const auto& factor = material.pbrData.baseColorFactor;
                        prim.BaseColorFactor = { factor.x(), factor.y(), factor.z(), factor.w() };
                        if (material.pbrData.baseColorTexture)
                            prim.BaseColorTexture = resolveTexture(material.pbrData.baseColorTexture->textureIndex);
                    }

                    model->m_Primitives.push_back(std::move(prim));
                }
            });

        EE_CORE_INFO("Loaded glTF model '{0}' with {1} primitives.",
            path.filename().string(), model->m_Primitives.size());
        return model;
    }
}
