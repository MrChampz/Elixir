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
