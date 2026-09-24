#include "Environment.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Logging/Log.h>

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    constexpr float Pi = 3.14159265359f;
    constexpr float TwoPi = 6.28318530718f;
    constexpr float InversePi = 0.31830988618f;
    constexpr float InverseTwoPi = 0.15915494309f;

    constexpr uint32_t IrradianceWidth = 48;
    constexpr uint32_t IrradianceHeight = 24;
    constexpr uint32_t IrradianceSamples = 256;
    constexpr uint32_t PrefilterWidth = 128;
    constexpr uint32_t PrefilterHeight = 64;
    constexpr uint32_t PrefilterSamples = 256;

    glm::vec3 EquirectangularToDirection(const float u, const float v)
    {
        const float phi = (u - 0.5f) * TwoPi;
        const float theta = v * Pi;
        const float sinTheta = std::sin(theta);
        return { sinTheta * std::cos(phi), std::cos(theta), sinTheta * std::sin(phi) };
    }

    glm::vec3 SampleEquirectangular(
        const float* pixels,
        const int width,
        const int height,
        const glm::vec3& direction
    )
    {
        const float u = std::atan2(direction.z, direction.x) * InverseTwoPi + 0.5f;
        const float v = std::acos(std::clamp(direction.y, -1.0f, 1.0f)) * InversePi;
        const int x = std::clamp(static_cast<int>(u * width), 0, width - 1);
        const int y = std::clamp(static_cast<int>(v * height), 0, height - 1);
        const float* pixel = pixels + static_cast<size_t>(y * width + x) * 4;
        return { pixel[0], pixel[1], pixel[2] };
    }

    float RadicalInverse(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    glm::vec3 ImportanceSampleGGX(
        const float xi0,
        const float xi1,
        const glm::vec3& normal,
        const float roughness
    )
    {
        const float alpha = roughness * roughness;
        const float phi = TwoPi * xi0;
        const float cosTheta = std::sqrt((1.0f - xi1) / (1.0f + (alpha * alpha - 1.0f) * xi1));
        const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
        const glm::vec3 halfVector(
            sinTheta * std::cos(phi),
            sinTheta * std::sin(phi),
            cosTheta
        );

        const glm::vec3 up = std::abs(normal.z) < 0.999f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
        const glm::vec3 bitangent = glm::cross(normal, tangent);
        return glm::normalize(
            tangent * halfVector.x + bitangent * halfVector.y + normal * halfVector.z
        );
    }

    std::vector<glm::vec4> BakeIrradiance(const float* pixels, const int width, const int height)
    {
        std::vector<glm::vec4> irradiance(IrradianceWidth * IrradianceHeight);

        for (uint32_t y = 0; y < IrradianceHeight; ++y)
        {
            for (uint32_t x = 0; x < IrradianceWidth; ++x)
            {
                const float u = (static_cast<float>(x) + 0.5f) / IrradianceWidth;
                const float v = (static_cast<float>(y) + 0.5f) / IrradianceHeight;
                const glm::vec3 normal = glm::normalize(EquirectangularToDirection(u, v));
                const glm::vec3 up = std::abs(normal.y) < 0.999f
                    ? glm::vec3(0.0f, 1.0f, 0.0f)
                    : glm::vec3(1.0f, 0.0f, 0.0f);
                const glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
                const glm::vec3 bitangent = glm::cross(normal, tangent);

                glm::vec3 sum(0.0f);
                for (uint32_t sample = 0; sample < IrradianceSamples; ++sample)
                {
                    const float xi0 = static_cast<float>(sample) / IrradianceSamples;
                    const float xi1 = RadicalInverse(sample);
                    const float phi = TwoPi * xi0;
                    const float cosTheta = std::sqrt(1.0f - xi1);
                    const float sinTheta = std::sqrt(xi1);
                    const glm::vec3 sampleDirection =
                        tangent * (sinTheta * std::cos(phi)) +
                        bitangent * (sinTheta * std::sin(phi)) +
                        normal * cosTheta;
                    sum += SampleEquirectangular(pixels, width, height, sampleDirection);
                }

                irradiance[y * IrradianceWidth + x] = glm::vec4(
                    sum / static_cast<float>(IrradianceSamples),
                    1.0f
                );
            }
        }

        return irradiance;
    }

    std::vector<glm::vec4> BakePrefiltered(
        const float* pixels,
        const int width,
        const int height
    )
    {
        std::vector<glm::vec4> prefiltered(
            PrefilterWidth * PrefilterHeight * Environment::PrefilterLevels
        );

        for (uint32_t level = 0; level < Environment::PrefilterLevels; ++level)
        {
            const float roughness = static_cast<float>(level) / (Environment::PrefilterLevels - 1);
            for (uint32_t y = 0; y < PrefilterHeight; ++y)
            {
                for (uint32_t x = 0; x < PrefilterWidth; ++x)
                {
                    const float u = (static_cast<float>(x) + 0.5f) / PrefilterWidth;
                    const float v = (static_cast<float>(y) + 0.5f) / PrefilterHeight;
                    const glm::vec3 normal = glm::normalize(EquirectangularToDirection(u, v));
                    const glm::vec3 view = normal;

                    glm::vec3 sum(0.0f);
                    float totalWeight = 0.0f;
                    for (uint32_t sample = 0; sample < PrefilterSamples; ++sample)
                    {
                        const float xi0 = static_cast<float>(sample) / PrefilterSamples;
                        const float xi1 = RadicalInverse(sample);
                        const glm::vec3 halfVector = ImportanceSampleGGX(
                            xi0,
                            xi1,
                            normal,
                            roughness
                        );
                        const glm::vec3 light = glm::normalize(
                            2.0f * glm::dot(view, halfVector) * halfVector - view
                        );
                        const float normalDotLight = std::max(glm::dot(normal, light), 0.0f);
                        if (normalDotLight > 0.0f)
                        {
                            sum += SampleEquirectangular(pixels, width, height, light) * normalDotLight;
                            totalWeight += normalDotLight;
                        }
                    }

                    const glm::vec3 color = totalWeight > 0.0f
                        ? sum / totalWeight
                        : SampleEquirectangular(pixels, width, height, normal);
                    const size_t index = (static_cast<size_t>(level) * PrefilterHeight + y) * PrefilterWidth + x;
                    prefiltered[index] = glm::vec4(color, 1.0f);
                }
            }
        }

        return prefiltered;
    }
}

Scope<Environment> Environment::Load(
    const GraphicsContext& context,
    const std::filesystem::path& path
)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    float* pixels = stbi_loadf(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        EE_CORE_ERROR("Failed to load HDR environment '{0}': {1}", path.string(), stbi_failure_reason())
        return nullptr;
    }

    auto environment = Scope<Environment>(new Environment());
    const auto* graphicsContext = &context;
    environment->m_Environment = Texture2D::Create(
        graphicsContext,
        EImageFormat::R32G32B32A32_SFLOAT,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        pixels,
        path.string()
    );

    const auto irradiance = BakeIrradiance(pixels, width, height);
    environment->m_Irradiance = Texture2D::Create(
        graphicsContext,
        EImageFormat::R32G32B32A32_SFLOAT,
        IrradianceWidth,
        IrradianceHeight,
        irradiance.data()
    );

    const auto prefiltered = BakePrefiltered(pixels, width, height);
    environment->m_Prefiltered = Texture2D::Create(
        graphicsContext,
        EImageFormat::R32G32B32A32_SFLOAT,
        PrefilterWidth,
        PrefilterHeight * PrefilterLevels,
        prefiltered.data()
    );
    stbi_image_free(pixels);

    environment->m_Sampler = SamplerBuilder()
        .SetMagFilter(ESamplerFilter::Linear)
        .SetMinFilter(ESamplerFilter::Linear)
        .SetMaxLod(GetMaxLod())
        .SetAddressModeU(ESamplerAddressMode::Repeat)
        .SetAddressModeV(ESamplerAddressMode::ClampToEdge)
        .SetAddressModeW(ESamplerAddressMode::ClampToEdge)
        .Build(graphicsContext);

    EE_CORE_INFO("Loaded HDR environment '{0}' ({1}x{2}).", path.filename().string(), width, height)
    return environment;
}
