#pragma once

#include <Engine/Core/Core.h>

#include <random>

namespace uuids
{
    class uuid;

    template <typename UniformRandomNumberGenerator>
    class basic_uuid_random_generator;

    using uuid_random_generator = basic_uuid_random_generator<std::mt19937>;
}

namespace Elixir
{
    class ELIXIR_API UUID
    {
      public:
        UUID();
        ~UUID();

        std::string ToString() const;

        bool operator==(const UUID& other) const;

        friend std::ostream& operator<<(std::ostream& os, const UUID& uuid)
        {
            os << uuid.ToString();
            return os;
        }

      private:
        Scope<uuids::uuid> m_UUID;

        static uuids::uuid_random_generator s_Generator;
    };
}

/**
 * Formatter for seamless UUID output in the logging system.
 */
template <>
struct std::formatter<Elixir::UUID> : std::formatter<std::string>
{
    auto format(const Elixir::UUID& uuid, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(uuid.ToString(), ctx);
    }
};