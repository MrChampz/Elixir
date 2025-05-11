#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-msc51-cpp"

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

    UUID::~UUID() = default;

    std::string UUID::ToString() const
    {
        return to_string(*m_UUID);
    }

    bool UUID::operator==(const UUID& other) const
    {
        return *m_UUID == *other.m_UUID;
    }
}

#pragma clang diagnostic pop