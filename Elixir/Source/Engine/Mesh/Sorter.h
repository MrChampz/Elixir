#pragma once

#include <Engine/Mesh/Mesh.h>

namespace Elixir
{
    /** Supplies the transforms shared by all sections of one mesh submission. */
    struct SSortContext
    {
        glm::mat4 Model{1.0f};
        glm::mat4 View{1.0f};
    };

    /**
     * @brief Define how mesh renderers calculate the sorting depth of a section.
     *
     * Supplies the SortDepth stored in a surface draw. Material rendering remains responsible
     * for ordering draws by that depth; this interface does not reorder geometry or triangles.
     * The renderer sets context once per mesh, then requests depths for its sections.
     * A sorter instance must not be used concurrently by multiple renderers.
     */
    class ELIXIR_API Sorter
    {
    public:
        virtual ~Sorter() = default;

        /**
         * @brief Prepare depth calculations for the sections of one mesh submission.
         * @param context Model and view transforms for the mesh being submitted.
         */
        virtual void SetContext(const SSortContext& context) = 0;

        /**
         * @brief Calculate the camera-space sorting depth of a mesh section.
         * @param section Section whose mesh-local bounds define the depth calculation.
         * @return Finite depth; larger values place translucent sections earlier.
         * @pre SetContext was called for the mesh being submitted.
         */
        virtual float CalculateDepth(const SMeshSection& section) const = 0;
    };

    /** Uses the section bounds center to preserve the mesh renderers' default depth. */
    class ELIXIR_API SectionSorter final : public Sorter
    {
    public:
        /**
         * @brief Combine model and view transforms once for the current mesh.
         * @param context Model and view transforms for the mesh being submitted.
         */
        void SetContext(const SSortContext& context) override;

        /**
         * @brief Calculate depth from the section bounds center transformed into camera space.
         * @param section Section with bounds in mesh-local space.
         * @return Negative camera-space Z of the center, positive in front of the camera.
         * @pre SetContext was called for the mesh being submitted.
         */
        float CalculateDepth(const SMeshSection& section) const override;

    private:
        // Cache the combined transform for all sections of the current mesh.
        glm::mat4 m_ModelView{1.0f};
    };
}
