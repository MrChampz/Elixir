#pragma once

namespace Elixir::Aether::Core
{
    /**
     * @brief Identifies a GPU particle-state layout supported by Aether.
     *
     * A layout defines the binary representation of one particle state in GPU
     * memory. The renderer selects compatible buffers, shaders, and pipelines from
     * this value.
     *
     * @note Layout values are part of the CPU-to-GPU contract and must remain
     * compatible with their corresponding shader declarations.
     */
    enum class EParticleStateLayout : uint8_t
    {
        CoreV1 = 0
    };

    // CoreV1 is six float4 values in both C++ and HLSL.
    constexpr uint32_t PARTICLE_STATE_CORE_V1_STRIDE = sizeof(glm::vec4) * 6;

    /**
     * @brief Describes the GPU memory requirements for one particle-state layout.
     *
     * The descriptor identifies a layout, the byte stride of one particle state,
     * and the maximum number of states that the renderer can allocate.
     */
    struct SParticleStateLayoutDescriptor
    {
        EParticleStateLayout Key = EParticleStateLayout::CoreV1;
        uint32_t ParticleStateStride = 0;
        uint32_t ParticleCapacity = 0;
    };

    /**
     * @brief Stores the particle-state layouts available to the Aether renderer.
     *
     * The registry is initialized before renderer resources are created. Each
     * registered descriptor must have compatible GPU buffers, shaders, and
     * pipelines in the renderer.
     *
     * @thread_safety Immutable after renderer initialization.
     */
    class ELIXIR_API ParticleStateLayoutRegistry final
    {
    public:
        /**
         * @brief Creates the registry with the built-in particle-state layouts.
         *
         * @param particleCapacity Maximum particle capacity assigned to each built-in
         * layout.
         */
        explicit ParticleStateLayoutRegistry(uint32_t particleCapacity);

        /**
         * @brief Registers one particle-state layout descriptor.
         *
         * @param descriptor Layout descriptor to register.
         * @return True when the descriptor was registered.
         * @return False when its key is already registered or its requirements are
         * invalid.
         *
         * @pre Register layouts before renderer initialization.
         */
        bool Register(SParticleStateLayoutDescriptor descriptor);

        /**
         * @brief Finds the descriptor for a particle-state layout.
         *
         * @param key Layout to find.
         * @return The matching descriptor, or null when the layout is unsupported.
         */
        const SParticleStateLayoutDescriptor* Find(EParticleStateLayout key) const;

        /**
         * @brief Returns all registered particle-state layout descriptors.
         * @return Read-only descriptors in registration order.
         */
        const std::vector<SParticleStateLayoutDescriptor>& GetDescriptors() const
        {
            return m_Descriptors;
        }

    private:
        std::vector<SParticleStateLayoutDescriptor> m_Descriptors;
    };
}
