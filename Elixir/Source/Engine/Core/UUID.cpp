#include "epch.h"
#include "UUID.h"

#include <uuid.h>

namespace Elixir
{
    using namespace uuids;

    std::mt19937 algorithm;
    uuid_random_generator UUID::s_Generator{algorithm};

    UUID::UUID()
    {
        m_UUID = CreateScope<uuid>(s_Generator());
    }

    UUID::UUID(const UUID& other)
    {
        EE_CORE_ASSERT(other.m_UUID, "Invalid UUID!");
        m_UUID = CreateScope<uuid>(*other.m_UUID);
    }

    UUID::UUID(UUID&& other) noexcept = default;

    UUID::~UUID() = default;

    UUID& UUID::operator=(const UUID& other)
    {
        EE_CORE_ASSERT(other.m_UUID, "Invalid UUID!")

        if (this == &other) return *this;

        m_UUID = CreateScope<uuid>(*other.m_UUID);
        return *this;
    }

    UUID& UUID::operator=(UUID&& other) noexcept = default;

    std::string UUID::ToString() const
    {
        return to_string(*m_UUID);
    }

    bool UUID::operator==(const UUID& other) const
    {
        return *m_UUID == *other.m_UUID;
    }

    std::size_t UUID::GetHashParams() const
    {
        EE_CORE_ASSERT(m_UUID, "Invalid UUID!")
        return Hash::Hash<uuid>(*m_UUID);
    }
}
