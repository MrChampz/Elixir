#pragma once

#include <Engine/Graphics/StaticMesh.h>

#include <glm/glm.hpp>

namespace Elixir
{
    /**
     * @brief References a static mesh with a transform local to its future owner.
     *
     * This class deliberately has no world transform or rendering state. A future ECS
     * component can combine its entity transform with this local transform.
     */
    class ELIXIR_API StaticMeshInstance final
    {
      public:
        /**
         * @brief Create an instance of one static mesh.
         * @param mesh Geometry referenced by the instance.
         * @param localTransform Transform relative to the instance owner.
         * @return An instance, or nullptr when mesh is null.
         */
        static Ref<StaticMeshInstance> Create(
            Ref<StaticMesh> mesh,
            const glm::mat4& localTransform = glm::mat4(1.0f)
        );

        /** @brief Get the static mesh referenced by this instance. */
        [[nodiscard]] const Ref<StaticMesh>& GetMesh() const { return m_Mesh; }
        /** @brief Get the transform relative to the instance owner. */
        [[nodiscard]] const glm::mat4& GetLocalTransform() const { return m_LocalTransform; }
        /**
         * @brief Replace the transform relative to the instance owner.
         * @param localTransform New local transform.
         */
        void SetLocalTransform(const glm::mat4& localTransform) { m_LocalTransform = localTransform; }

      private:
        StaticMeshInstance(Ref<StaticMesh> mesh, const glm::mat4& localTransform);

        Ref<StaticMesh> m_Mesh;
        glm::mat4 m_LocalTransform{ 1.0f };
    };
}
