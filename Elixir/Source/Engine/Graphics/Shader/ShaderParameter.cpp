#include "epch.h"
#include "ShaderParameter.h"

namespace Elixir
{
    /**
     * ShaderResource
     */

    ShaderResource::ShaderResource(
        const std::string& name,
        const uint32_t set,
        const uint32_t binding,
        const EResourceType type,
        const EResourceDimension dimension,
        const uint32_t count,
        const bool bindless
    ) : m_Name(name),
        m_Set(set),
        m_Binding(binding),
        m_Type(type),
        m_Dimension(dimension),
        m_Count(count),
        m_IsBindless(bindless) {}

    /**
     * ShaderConstant
     */

    ShaderConstant::ShaderConstant(
        const std::string& name,
        const EConstantType type,
        const uint32_t count,
        const bool pointer
    ) : m_Name(name), m_Type(type), m_Count(count), m_Pointer(pointer)
    {
        m_Size = pointer ? POINTER_SIZE : Utils::GetShaderConstantTypeSize(type) * count;
    }

    ShaderConstant::ShaderConstant(
        const std::string& name,
        Ref<ShaderConstantStruct> structure,
        const uint32_t count,
        const bool pointer
    )
        : m_Name(name),
        m_Type(EConstantType::Struct),
        m_Struct(std::move(structure)),
        m_Count(count),
        m_Pointer(pointer)
    {
        m_Size = pointer ? POINTER_SIZE : structure->GetSize() * count;
    }

    void ShaderConstant::SetOffset(const uint32_t offset)
    {
        if (m_Type == EConstantType::Struct)
        {
            m_Struct->SetOffset(offset);
        }

        m_Offset = offset;
    }

    /**
     * ShaderConstantStruct
     */

    void ShaderConstantStruct::AddField(ShaderConstant&& field)
    {
        m_Size += field.GetSize();

        uint32_t offset = 0;
        if (m_Fields.size() > 0)
        {
            const auto& previous = m_Fields.back();
            offset = previous.GetOffset() + previous.GetSize();
        }

        field.SetOffset(offset);
        m_Fields.push_back(std::move(field));
    }

    /**
     * ShaderConstantBuffer
     */

    ShaderConstantBuffer::ShaderConstantBuffer(
        const std::string& name,
        const uint32_t set,
        const uint32_t binding
    ) : m_Name(name), m_Set(set), m_Binding(binding) {}

    void ShaderConstantBuffer::PushConstant(ShaderConstant&& constant)
    {
        uint32_t offset = 0;

        if (m_Constants.size())
        {
            const auto& previous = m_Constants.back();
            offset = previous.GetOffset() + previous.GetSize();
        }

        constant.SetOffset(offset);
        m_Size += constant.GetSize();
        m_Constants.push_back(std::move(constant));
    }

    const ShaderConstant* ShaderConstantBuffer::FindConstant(const std::string& name)
    {
        const auto it = std::ranges::find_if(
            m_Constants,
            [&](const auto& constant)
            {
                return constant.GetName() == name;
            }
        );
        return it != m_Constants.end() ? &(*it) : nullptr;
    }

    /**
     * ShaderPushConstant
     */

    ShaderPushConstant::ShaderPushConstant(
        const std::string& name,
        const uint32_t set,
        const uint32_t binding,
        const uint32_t offset
    ) : m_Name(name), m_Set(set), m_Binding(binding), m_Offset(offset) {}

    void ShaderPushConstant::PushConstant(ShaderConstant&& constant)
    {
        uint32_t offset = 0;

        if (m_Constants.size())
        {
            const auto& previous = m_Constants.back();
            offset = previous.GetOffset() + previous.GetSize();
        }

        constant.SetOffset(offset);
        m_Size += constant.GetSize();
        m_Constants.push_back(std::move(constant));
    }

    const ShaderConstant* ShaderPushConstant::FindConstant(const std::string& name)
    {
        const auto it = std::ranges::find_if(
            m_Constants,
            [&](const auto& constant)
            {
                return constant.GetName() == name;
            }
        );
        return it != m_Constants.end() ? &(*it) : nullptr;
    }
}