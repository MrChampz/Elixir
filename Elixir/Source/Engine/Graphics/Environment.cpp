#include "epch.h"
#include "Environment.h"

#include <stb_image.h>

namespace Elixir
{
    // Equirectangular mapping, matching the shader:
    //   u = atan2(z, x) / 2pi + 0.5,  v = acos(y) / pi   (y up, v=0 at the top)
    static glm::vec3 EquirectToDir(float u, float v)
    {
        const float phi = (u - 0.5f) * 6.28318530718f;
        const float theta = v * 3.14159265359f;
        const float sinT = std::sin(theta);
        return { sinT * std::cos(phi), std::cos(theta), sinT * std::sin(phi) };
    }

    static glm::vec3 SampleEquirect(const float* pixels, int w, int h, const glm::vec3& dir)
    {
        const float u = std::atan2(dir.z, dir.x) * 0.15915494f + 0.5f;
        const float v = std::acos(std::clamp(dir.y, -1.0f, 1.0f)) * 0.31830989f;
        int x = std::clamp((int)(u * w), 0, w - 1);
        int y = std::clamp((int)(v * h), 0, h - 1);
        const float* p = pixels + (size_t)(y * w + x) * 4;
        return { p[0], p[1], p[2] };
    }

    static float RadicalInverse(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return (float)bits * 2.3283064365386963e-10f;
    }

    Ref<Environment> Environment::Load(const GraphicsContext* context, const std::filesystem::path& hdrPath)
    {
        int w = 0, h = 0, channels = 0;
        float* pixels = stbi_loadf(hdrPath.string().c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            EE_CORE_ERROR("Failed to load HDR environment '{0}': {1}", hdrPath.string(), stbi_failure_reason());
            return nullptr;
        }

        auto environment = CreateRef<Environment>();
        environment->m_Environment = Texture2D::Create(
            context, EImageFormat::R32G32B32A32_SFLOAT, (uint32_t)w, (uint32_t)h, pixels);

        // Bake a small diffuse-irradiance map: cosine-weighted hemisphere
        // convolution of the environment for each output direction.
        constexpr int IW = 48, IH = 24;
        constexpr uint32_t SAMPLES = 256;
        std::vector<glm::vec4> irradiance((size_t)IW * IH);

        for (int y = 0; y < IH; ++y)
        {
            for (int x = 0; x < IW; ++x)
            {
                const float u = (x + 0.5f) / IW;
                const float v = (y + 0.5f) / IH;
                const glm::vec3 N = glm::normalize(EquirectToDir(u, v));

                const glm::vec3 up = std::abs(N.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                const glm::vec3 T = glm::normalize(glm::cross(up, N));
                const glm::vec3 B = glm::cross(N, T);

                glm::vec3 sum(0.0f);
                for (uint32_t i = 0; i < SAMPLES; ++i)
                {
                    const float xi0 = (float)i / SAMPLES;
                    const float xi1 = RadicalInverse(i);
                    const float phi = 6.28318530718f * xi0;
                    const float cosTheta = std::sqrt(1.0f - xi1);
                    const float sinTheta = std::sqrt(xi1);
                    const glm::vec3 tangent(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
                    const glm::vec3 dir = tangent.x * T + tangent.y * B + tangent.z * N;
                    sum += SampleEquirect(pixels, w, h, dir);
                }

                irradiance[(size_t)y * IW + x] = glm::vec4(sum / (float)SAMPLES, 1.0f);
            }
        }

        environment->m_Irradiance = Texture2D::Create(
            context, EImageFormat::R32G32B32A32_SFLOAT, IW, IH, irradiance.data());

        stbi_image_free(pixels);

        EE_CORE_INFO("Loaded HDR environment '{0}' ({1}x{2}).", hdrPath.filename().string(), w, h);
        return environment;
    }
}
