#pragma once

#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    class Camera;
    class GraphicsContext;
    class ShaderLoader;
    class GeometryPool;

    /**
     * @brief Renders static meshes with one stable pseudo-random color per mesh.
     *
     * This renderer is a temporary visualisation path. It does not resolve materials,
     * scene transforms, lighting, or ECS data.
     */
    class ELIXIR_API StaticMeshRenderer final
    {
      public:
        /**
         * @brief Create a static mesh renderer for one graphics context.
         * @param context Graphics context used to record commands and create the pipeline.
         * @param shaderLoader Loader used to create the visualization shader.
         * @param geometryPool Pool that stores the geometry referenced by rendered meshes.
         * @pre context, shaderLoader, and geometryPool remain valid for the renderer lifetime.
         */
        StaticMeshRenderer(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const GeometryPool& geometryPool
        );

        /**
         * @brief Begin recording static mesh draws for one frame.
         *
         * Updates camera data, begins rendering, configures viewport and scissor,
         * and binds the pipeline and shared geometry buffers.
         *
         * @param camera Camera used to build the view-projection matrix.
         * @pre EndFrame was called after the previous BeginFrame.
         */
        void BeginFrame(const Camera& camera);

        /**
         * @brief Record draw commands for one static mesh.
         *
         * Each section produces one indexed draw using its geometry range and
         * material slot. The mesh remains in mesh-local space.
         *
         * @param mesh Static mesh to draw.
         * @pre BeginFrame was called and EndFrame was not called yet.
         */
        void Render(const Ref<StaticMesh>& mesh);

        /**
         * @brief Finish recording and submit static mesh draws for the current frame.
         *
         * @pre BeginFrame was called and EndFrame was not called yet.
         */
        void EndFrame();

      private:
        /** Return a stable pseudo-random color for one mesh identity. */
        glm::vec4 GetColor(const StaticMesh& mesh);

        const GraphicsContext* m_Context = nullptr;
        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<UniformBuffer> m_FrameBuffer;
        Ref<CommandBuffer> m_CommandBuffer;
        const GeometryPool& m_GeometryPool;

        std::unordered_map<const StaticMesh*, glm::vec4> m_Colors;
    };
}
