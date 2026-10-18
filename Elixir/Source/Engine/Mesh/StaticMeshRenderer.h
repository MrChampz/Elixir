#pragma once

#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/Mesh/MeshLighting.h>
#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Mesh/Sorter.h>
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
     * @brief Collect static mesh surface draws for MaterialSystem.
     *
     * Reuses geometry already uploaded to an external pool. Each frame starts with
     * BeginFrame, collects meshes with Render, and submits their draws with EndFrame.
     */
    class ELIXIR_API StaticMeshRenderer final
    {
      public:
        /**
         * @brief Create a renderer that reads static geometry from an external pool.
         * @param context Graphics context used to create camera and lighting buffers.
         * @param materialSystem Receives the collected surface draws at EndFrame.
         * @param geometryPool Pool containing the rendered meshes in SStaticMeshVertex layout.
         * @pre The context, material system, and geometry pool outlive this renderer.
         */
        StaticMeshRenderer(
            const GraphicsContext& context,
            MaterialSystem& materialSystem,
            const GeometryPool& geometryPool
        );

        /**
         * @brief Begin collecting static mesh draws for the current frame slot.
         *
         * Updates camera and lighting data and binds the shared geometry buffers.
         * The geometry pool must be valid and use the SStaticMeshVertex stride.
         *
         * @param camera Supplies view, projection, position, and section sorting depths.
         * @param lighting Surface lighting and environment; light direction must be nonzero.
         * @pre The previous frame was closed and the current slot's GPU work completed.
         */
        void BeginFrame(
            const Camera& camera,
            const SMeshLighting& lighting
        );

        /**
         * @brief Collect indexed surface draws for one static mesh.
         *
         * Each section with a usable material produces one draw. The sorter calculates its
         * depth; SectionSorter uses the section bounds center in camera space by default.
         * Sections without a usable material are skipped without rejecting the mesh.
         *
         * @param mesh Mesh whose geometry was uploaded to this renderer's external pool.
         * @param transform Model-to-world matrix used by the draw and depth strategy.
         * @return True if the mesh was accepted, even if all its sections were skipped.
         * Returns false if the frame is closed, the mesh is null, or its geometry is invalid.
         * @pre BeginFrame opened the frame and EndFrame has not closed it.
         */
        bool Render(
            const Ref<const StaticMesh>& mesh,
            const glm::mat4& transform = glm::mat4(1)
        );

        /**
         * @brief Close the frame and submit its collected draws to MaterialSystem.
         * @pre BeginFrame opened the frame and EndFrame has not closed it.
         */
        void EndFrame();

        /**
         * @brief Select the depth strategy used by subsequent Render calls.
         * @param sorter Depth sorter; if null is provided, do nothing.
         */
        void SetSorter(Scope<Sorter> sorter);

      private:
        /**
         * @brief Reuse or create the default instance for a material.
         * @param material Material to cache; null is accepted and returns null.
         * @return Cached instance, or null if the material is absent or creation fails.
         */
        Ref<MaterialInstance> GetDefaultInstance(const Ref<Material>& material);

        Scope<Sorter> m_Sorter;

        // Keep camera and lighting uploads separate for each frame slot in flight.
        FrameSlotState<Ref<UniformBuffer>> m_FrameBuffers;

        const GeometryPool& m_GeometryPool;
        // Index of the shared pool buffers in the current material scene.
        uint32_t m_GeometryIndex = UINT32_MAX;

        MaterialSystem& m_MaterialSystem;
        Rendering::MaterialRenderScene m_Scene;
        std::unordered_map<const Material*, Ref<MaterialInstance>> m_DefaultInstances;

        glm::mat4 m_View{ 1.0f };

        bool m_Recording = false;

        const GraphicsContext& m_Context;
    };
}
