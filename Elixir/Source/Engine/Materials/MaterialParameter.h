#pragma once

namespace Elixir::Materials
{
    /**
     * @brief Identifies the category of a material parameter.
     */
    enum class EMaterialParameterKind : uint8_t
    {
        /** A scalar or vector parameter. */
        Value,

        /** A texture parameter. */
        Texture
    };

    /**
     * @brief Identifies the value stored by a material parameter.
     */
    enum class EMaterialParameterType : uint8_t
    {
        /** A single floating-point value. */
        Scalar,

        /** A four-component floating-point value. */
        Vector,

        /** A texture reference. */
        Texture
    };

    /**
     * @brief Stores one material parameter value.
     *
     * @ref Type identifies which value member is active.
     */
    struct SMaterialParameter
    {
        /** Type of the active value. */
        EMaterialParameterType Type = EMaterialParameterType::Scalar;

        /** Scalar value when @ref Type is `Scalar`. */
        float Scalar = 0.0f;

        /** Vector value when @ref Type is `Vector`. */
        glm::vec4 Vector{ 0.0f };

        /** Texture value when @ref Type is `Texture`. */
        Ref<Texture> Texture;

        /**
         * @brief Creates a scalar material parameter.
         * @param value Scalar value to store.
         * @return A parameter whose type is `Scalar`.
         */
        static SMaterialParameter MakeScalar(const float value)
        {
            SMaterialParameter param;
            param.Type = EMaterialParameterType::Scalar;
            param.Scalar = value;
            return param;
        }

        /**
         * @brief Creates a vector material parameter.
         * @param value Vector value to store.
         * @return A parameter whose type is `Vector`.
         */
        static SMaterialParameter MakeVector(const glm::vec4& value)
        {
            SMaterialParameter param;
            param.Type = EMaterialParameterType::Vector;
            param.Vector = value;
            return param;
        }

        /**
         * @brief Creates a texture material parameter.
         * @param texture Texture to store.
         * @return A parameter whose type is `Texture`.
         */
        static SMaterialParameter MakeTexture(const Ref<Elixir::Texture>& texture)
        {
            SMaterialParameter param;
            param.Type = EMaterialParameterType::Texture;
            param.Texture = texture;
            return param;
        }
    };
}