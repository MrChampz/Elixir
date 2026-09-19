#pragma once

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
         * @pre EndFrame was called after the previous BeginFrame.
         */
        void BeginFrame(const Camera& camera);

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

        const GraphicsContext* m_Context = nullptr;
    };
}
