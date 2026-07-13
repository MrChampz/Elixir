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

    // Decode compressed image bytes (png/jpg) into an RGBA texture.
    static Ref<Texture> DecodeImage(
        const GraphicsContext* context, const std::byte* bytes, size_t size, EImageFormat format)
    {
        int w = 0, h = 0, channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes), (int)size, &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            EE_CORE_WARN("Failed to decode embedded glTF image: {0}", stbi_failure_reason());
            return nullptr;
        }

        auto texture = Texture2D::Create(context, format, (uint32_t)w, (uint32_t)h, pixels);
        stbi_image_free(pixels);
        return texture;
    }

    // Resolve a glTF image (external URI or embedded buffer/bufferView) to a texture.
    static Ref<Texture> LoadImage(
        const GraphicsContext* context,
        fastgltf::Asset& asset,
        size_t imageIndex,
        const std::filesystem::path& dir,
        EImageFormat format)
    {
        Ref<Texture> result;
        auto& image = asset.images[imageIndex];

        std::visit(fastgltf::visitor{
            [](auto&) {},
            [&](fastgltf::sources::URI& uri)
            {
                const auto imagePath = dir / uri.uri.fspath();
                if (std::filesystem::exists(imagePath))
                    result = TextureLoader::Load(imagePath, format);
                else
                    EE_CORE_WARN("glTF texture not found: {0}", imagePath.string());
            },
            [&](fastgltf::sources::Array& arr)
            {
                result = DecodeImage(context, arr.bytes.data(), arr.bytes.size(), format);
            },
            [&](fastgltf::sources::Vector& vec)
            {
                result = DecodeImage(context, vec.bytes.data(), vec.bytes.size(), format);
            },
            [&](fastgltf::sources::BufferView& view)
            {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];
                std::visit(fastgltf::visitor{
                    [](auto&) {},
                    [&](fastgltf::sources::Array& arr)
                    {
                        result = DecodeImage(context, arr.bytes.data() + bufferView.byteOffset, bufferView.byteLength, format);
                    },
                    [&](fastgltf::sources::Vector& vec)
                    {
                        result = DecodeImage(context, vec.bytes.data() + bufferView.byteOffset, bufferView.byteLength, format);
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

        // Extensions must be enabled explicitly or the parser drops them
        // (KHR_texture_transform: textures render untiled; emissive_strength:
        // emissive intensity multiplier is lost).
        fastgltf::Parser parser(
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_emissive_strength |
            fastgltf::Extensions::KHR_materials_clearcoat |
            fastgltf::Extensions::KHR_materials_specular);
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

        // Cache textures by (glTF texture index, sRGB) so shared textures load once.
        std::unordered_map<uint64_t, Ref<Texture>> textureCache;
        auto loadTexture = [&](size_t textureIndex, bool srgb) -> Ref<Texture>
        {
            const uint64_t key = (uint64_t)textureIndex << 1 | (srgb ? 1ull : 0ull);
            if (const auto it = textureCache.find(key); it != textureCache.end())
                return it->second;

            Ref<Texture> texture;
            const auto& gltfTexture = asset.textures[textureIndex];
            if (gltfTexture.imageIndex)
            {
                const auto format = srgb ? EImageFormat::R8G8B8A8_SRGB : EImageFormat::R8G8B8A8_UNORM;
                texture = LoadImage(context, asset, *gltfTexture.imageIndex, dir, format);
            }

            textureCache[key] = texture;
            return texture;
        };

        // KHR_texture_transform -> (uv scale.xy, uv offset.xy); identity if absent.
        auto getTransform = [](const std::unique_ptr<fastgltf::TextureTransform>& t) -> glm::vec4
        {
            if (!t)
                return glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            return glm::vec4(t->uvScale[0], t->uvScale[1], t->uvOffset[0], t->uvOffset[1]);
        };

        // Build the material table as instances of the built-in StandardPBR material.
        // Each glTF material overrides only the parameters it specifies; the rest fall
        // back to the parent defaults. Base-color/emissive are colour (sRGB); the
        // metallic-roughness, normal and occlusion maps are data (linear).
        const auto standardPBR = Material::CreateStandardPBR();
        model->m_Materials.reserve(asset.materials.size() + 1);
        for (auto& material : asset.materials)
        {
            auto instance = CreateRef<MaterialInstance>(standardPBR);

            const auto& bcf = material.pbrData.baseColorFactor;
            instance->SetVector("BaseColorFactor", { bcf.x(), bcf.y(), bcf.z(), bcf.w() });
            const auto& ef = material.emissiveFactor;
            instance->SetVector("EmissiveFactor",
                glm::vec4(glm::vec3(ef.x(), ef.y(), ef.z()) * material.emissiveStrength, 0.0f));
            instance->SetScalar("Metallic", material.pbrData.metallicFactor);
            instance->SetScalar("Roughness", material.pbrData.roughnessFactor);
            instance->SetScalar("AlphaCutoff", material.alphaCutoff);
            instance->SetScalar("AlphaMode", material.alphaMode == fastgltf::AlphaMode::Blend ? 2.0f
                : material.alphaMode == fastgltf::AlphaMode::Mask ? 1.0f : 0.0f);

            if (material.clearcoat)
            {
                instance->SetScalar("ClearcoatFactor", material.clearcoat->clearcoatFactor);
                instance->SetScalar("ClearcoatRoughness", material.clearcoat->clearcoatRoughnessFactor);
            }

            if (material.specular)
            {
                instance->SetScalar("SpecularFactor", material.specular->specularFactor);
                const auto& scf = material.specular->specularColorFactor;
                instance->SetVector("SpecularColorFactor", glm::vec4(scf.x(), scf.y(), scf.z(), 1.0f));
                if (material.specular->specularTexture)
                    instance->SetTexture("SpecularTexture", loadTexture(material.specular->specularTexture->textureIndex, false));
                if (material.specular->specularColorTexture)
                    instance->SetTexture("SpecularColorTexture", loadTexture(material.specular->specularColorTexture->textureIndex, true));
            }

            if (material.pbrData.baseColorTexture)
            {
                instance->SetTexture("BaseColorTexture", loadTexture(material.pbrData.baseColorTexture->textureIndex, true));
                instance->SetVector("BaseColorTransform", getTransform(material.pbrData.baseColorTexture->transform));
            }
            if (material.pbrData.metallicRoughnessTexture)
            {
                instance->SetTexture("MetallicRoughnessTexture", loadTexture(material.pbrData.metallicRoughnessTexture->textureIndex, false));
                instance->SetVector("MetallicRoughnessTransform", getTransform(material.pbrData.metallicRoughnessTexture->transform));
            }
            if (material.emissiveTexture)
            {
                instance->SetTexture("EmissiveTexture", loadTexture(material.emissiveTexture->textureIndex, true));
                instance->SetVector("EmissiveTransform", getTransform(material.emissiveTexture->transform));
            }
            if (material.normalTexture)
            {
                instance->SetScalar("NormalScale", material.normalTexture->scale);
                instance->SetTexture("NormalTexture", loadTexture(material.normalTexture->textureIndex, false));
                instance->SetVector("NormalTransform", getTransform(material.normalTexture->transform));
            }
            if (material.occlusionTexture)
            {
                instance->SetScalar("OcclusionStrength", material.occlusionTexture->strength);
                instance->SetTexture("OcclusionTexture", loadTexture(material.occlusionTexture->textureIndex, false));
                instance->SetVector("OcclusionTransform", getTransform(material.occlusionTexture->transform));
            }

            model->m_Materials.push_back(instance);
        }

        // Fallback material for primitives without one (all defaults).
        const uint32_t defaultMaterialIndex = (uint32_t)model->m_Materials.size();
        model->m_Materials.push_back(CreateRef<MaterialInstance>(standardPBR));

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
                            vertices[i].Tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
                            vertices[i].TexCoord = { 0.0f, 0.0f };
                        });

                    if (const auto* tanAttr = primitive.findAttribute("TANGENT");
                        tanAttr != primitive.attributes.end())
                    {
                        auto& accessor = asset.accessors[tanAttr->accessorIndex];
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                            asset, accessor, [&](fastgltf::math::fvec4 v, size_t i)
                            {
                                vertices[i].Tangent = { v.x(), v.y(), v.z(), v.w() };
                            });
                    }

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
                    prim.MaterialIndex = primitive.materialIndex
                        ? (uint32_t)*primitive.materialIndex
                        : defaultMaterialIndex;
                    prim.Vertices = VertexBuffer::Create(
                        context, vertices.size() * sizeof(SModelVertex), vertices.data());
                    prim.Indices = IndexBuffer::Create(
                        context, indices.size() * sizeof(uint32_t), indices.data(), EIndexType::UInt32);

                    model->m_Primitives.push_back(std::move(prim));
                }
            });

        EE_CORE_INFO("Loaded glTF model '{0}': {1} primitives, {2} materials.",
            path.filename().string(), model->m_Primitives.size(), model->m_Materials.size());
        return model;
    }
}
