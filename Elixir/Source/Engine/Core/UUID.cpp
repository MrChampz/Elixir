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
        if (other.m_UUID)
            m_UUID = CreateScope<uuid>(*other.m_UUID);
    }

    UUID::UUID(UUID&& other) noexcept = default;

    UUID::~UUID() = default;

    UUID& UUID::operator=(const UUID& other)
    {
        if (this == &other)
            return *this;

        m_UUID = other.m_UUID ? CreateScope<uuid>(*other.m_UUID) : nullptr;
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
}