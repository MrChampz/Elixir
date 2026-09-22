#pragma once

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Sampler.h>
#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>

namespace Elixir
{
    namespace Materials { class MaterialSystem; }
    using namespace Materials;

    class Camera;
    class GraphicsContext;
    class ShaderLoader;
    class GeometryPool;

    /**
     * @brief Stores image-based lighting resources for Surface materials.
     *
     * All textures use equirectangular projection. The prefiltered texture stores
     * roughness-filtered specular reflections.
     */
    struct SSurfaceEnvironment
    {
        /** Raw environment texture used for sharp reflections. */
        Ref<Texture> Environment;

        /** Diffuse irradiance texture derived from @ref Environment. */
        Ref<Texture> Irradiance;

        /** Roughness-filtered specular reflection texture. */
        Ref<Texture> Prefiltered;

        /** Sampler used by all environment textures. */
        Ref<Sampler> Sampler;

        /** Multiplier applied to environment lighting. */
        float Intensity = 1.0f;

        /** Highest valid mip level in @ref Environment. */
        float MaxLod = 0.0f;
    };

    /** @brief Stores frame lighting used by static mesh Surface materials. */
    struct SStaticMeshLighting
    {
        /** Image-based lighting resources for the frame. */
        SSurfaceEnvironment Environment;

        /** Direction from the shared point toward the directional light. */
        glm::vec3 DirectionalLightDirection{ -0.5f, 0.65f, -0.55f };

        /** Linear RGB color of the directional light. */
        glm::vec3 DirectionalLightColor{ 1.0f, 0.96f, 0.9f };

        /** Intensity multiplier of the directional light. */
        float DirectionalLightIntensity = 2.2f;
    };

    /**
     * @brief Collects static mesh surface draws for MaterialSystem.
     */
    class ELIXIR_API StaticMeshRenderer final
    {
      public:
        /**
         * @brief Create a static mesh renderer for one graphics context.
         * @param context Graphics context used to create frame resources.
         * @param materialSystem Material system that records submitted surface draws.
         * @param geometryPool Pool that stores the geometry referenced by rendered meshes.
         * @pre All arguments remain valid for the renderer lifetime.
         */
        StaticMeshRenderer(
            const GraphicsContext* context,
            MaterialSystem& materialSystem,
            const GeometryPool& geometryPool
        );

        /**
         * @brief Begin recording static mesh draws for one frame.
         *
         * Updates camera data and begins collecting material draw commands.
         *
         * @param camera Camera used to build the view-projection matrix.
         * @param lighting Lighting resources used by Surface materials.
         * @pre EndFrame was called after the previous BeginFrame.
         */
        void BeginFrame(
            const Camera& camera,
            const SStaticMeshLighting& lighting
        );

        /**
         * @brief Record draw commands for one static mesh.
         *
         * Each section produces one indexed surface draw.
         *
         * @param mesh Static mesh to draw.
         * @pre BeginFrame was called and EndFrame was not called yet.
         */
        void Render(const Ref<StaticMesh>& mesh);

        /**
         * @brief Submit collected static mesh draws to MaterialSystem.
         * @pre BeginFrame was called and EndFrame was not called yet.
         */
        void EndFrame();

      private:
        Ref<MaterialInstance> GetDefaultInstance(const Ref<Material>& material);

        MaterialSystem& m_MaterialSystem;
        Ref<UniformBuffer> m_FrameBuffer;
        const GeometryPool& m_GeometryPool;

        Rendering::MaterialRenderScene m_Scene;
        uint32_t m_GeometryIndex = UINT32_MAX;
        std::unordered_map<const Material*, Ref<MaterialInstance>> m_DefaultInstances;
        glm::mat4 m_View{ 1.0f };

        const GraphicsContext* m_Context = nullptr;
    };
}
