#include "epch.h"
#include "BufferLayout.h"

namespace Elixir
{
    /* BufferAttribute */

    BufferAttribute::BufferAttribute(
        const EDataType type,
        const std::string& name,
        const bool normalized
    ) : m_Name(name), m_Type(type), m_Offset(0), m_Size(Utils::GetDataTypeSize(type)),
        m_Normalized(normalized) {}

    uint32_t BufferAttribute::GetComponentCount() const
    {
        return Utils::GetDataTypeComponentCount(m_Type);
    }

    /* BufferBinding */

    BufferBinding::BufferBinding(
        const EInputRate inputRate,
        const std::initializer_list<BufferAttribute>& attributes
    ) : m_InputRate(inputRate), m_Attributes(attributes)
    {
        CalculateOffsetsAndStride();
    }

    void BufferBinding::CalculateOffsetsAndStride()
    {
        m_Stride = 0;

        size_t offset = 0;
        for (auto& attribute : m_Attributes)
        {
            attribute.m_Offset = offset;
            offset += attribute.m_Size;
            m_Stride += attribute.m_Size;
        }
    }

    /* BufferLayout */

    BufferLayout::BufferLayout(const std::initializer_list<BufferBinding>& bindings)
    {
        std::ranges::for_each(bindings, [this](BufferBinding binding)
        {
            binding.m_Binding = (uint32_t)m_Bindings.size();
            m_Bindings.push_back(std::move(binding));
        });
    }

    void BufferLayout::AddBinding(BufferBinding&& binding)
    {
        binding.m_Binding = (uint32_t)m_Bindings.size();
        m_Bindings.push_back(std::move(binding));
    }

    const BufferBinding* BufferLayout::GetBinding(const uint32_t binding) const
    {
        for (auto& b : m_Bindings)
        {
            if (b.GetBinding() == binding)
                return &b;
        }

        EE_CORE_ERROR("The specified binding was not found [Binding={}]", binding)
        return nullptr;
    }

    size_t BufferLayout::GetAttributeCount() const
    {
        size_t count = 0;

        for (const auto& binding : m_Bindings)
        {
            count += binding.GetAttributes().size();
        }

        return count;
    }
}