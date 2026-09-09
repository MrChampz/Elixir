#pragma once

#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    class Camera;
    class GraphicsContext;
    class ShaderLoader;

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
         * @pre context and shaderLoader remain valid for the renderer lifetime.
         */
        StaticMeshRenderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        /**
         * @brief Record draws for all supplied meshes using the camera view projection.
         * @param meshes Meshes to draw in mesh-local space.
         * @param camera Camera used to project mesh vertices.
         */
        void Render(std::span<const Ref<StaticMesh>> meshes, const Camera& camera);

      private:
        /** Return a stable pseudo-random color for one mesh identity. */
        glm::vec4 GetColor(const StaticMesh& mesh);

        const GraphicsContext* m_Context = nullptr;
        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<UniformBuffer> m_FrameBuffer;
        std::unordered_map<const StaticMesh*, glm::vec4> m_Colors;
    };
}
