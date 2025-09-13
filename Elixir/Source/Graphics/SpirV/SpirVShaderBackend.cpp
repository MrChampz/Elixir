#include "epch.h"
#include "SpirVShaderBackend.h"

#include <Engine/Graphics/Shader/Shader.h>

#include <fstream>
#include <spirv_cross.hpp>

namespace Elixir::SpirV
{
    std::vector<uint32_t> LoadSpirVFile(const std::filesystem::path& filepath)
    {
        const auto size = file_size(filepath);
        if (size % sizeof(uint32_t) != 0)
        {
            EE_CORE_ASSERT(false, "Invalid SPIR-V file, size is not 4-byte aligned.")
            return {};
        }

        std::vector<uint32_t> spirv(size / sizeof(uint32_t));

        std::ifstream file(filepath, std::ios::binary);
        if (!file.read((char*)spirv.data(), size))
        {
            EE_CORE_ASSERT(false, "Failed to read SPIR-V file.")
            return {};
        }

        file.close();

        return spirv;
    }

    std::vector<Byte> ConvertBytecode(const std::vector<uint32_t>& spirv)
    {
        auto src = std::as_bytes(std::span{spirv});
        return {src.begin(), src.end()};
    }

    SShaderModuleCreateInfo SpirVShaderBackend::LoadModule(
        const std::filesystem::path& filepath,
        const EShaderStage stage
    ) const
    {
        const auto spirv = LoadSpirVFile(filepath);
        return Load(spirv, filepath);
    }

    SShaderModuleCreateInfo SpirVShaderBackend::Load(
        const std::vector<uint32_t>& spirv,
        const std::filesystem::path& filepath
    )
    {
        const spirv_cross::Compiler compiler(spirv);

        SShaderModuleCreateInfo module = {};
        module.Path = filepath;
        module.Entrypoint = GetEntrypoint(compiler);
        module.Bytecode = ConvertBytecode(spirv);

        auto resources = compiler.get_shader_resources();
        module.Resources = GetResources(compiler, resources);
        module.ConstantBuffers = GetConstantBuffers(compiler, resources);
        module.PushConstants = GetPushConstants(compiler, resources);

        return module;
    }

    std::string SpirVShaderBackend::GetEntrypoint(const spirv_cross::Compiler& spirv)
    {
        auto entrypoints = spirv.get_entry_points_and_stages();
        return entrypoints[0].name;
    }

    std::vector<ShaderResource> SpirVShaderBackend::GetResources(
        const spirv_cross::Compiler& spirv,
        spirv_cross::ShaderResources& resources
    )
    {
        std::vector<ShaderResource> result;

        for (const auto& resource : resources.separate_images)
        {
            auto& name = spirv.get_name(resource.id);
            const auto set = spirv.get_decoration(resource.id, spv::DecorationDescriptorSet);
            const auto binding = spirv.get_decoration(resource.id, spv::DecorationBinding);

            const auto& type = spirv.get_type(resource.type_id);
            const auto& baseType = spirv.get_type(resource.base_type_id);

            const auto count = CalculateResourceCount(type);
            const auto dimension = GetResourceDimension(baseType.image.dim);

            auto image = ShaderResource(name, set, binding, EResourceType::Image, dimension, count);
            result.push_back(std::move(image));
        }

        for (const auto& resource : resources.sampled_images)
        {
            auto& name = spirv.get_name(resource.id);
            const auto set = spirv.get_decoration(resource.id, spv::DecorationDescriptorSet);
            const auto binding = spirv.get_decoration(resource.id, spv::DecorationBinding);

            const auto& type = spirv.get_type(resource.type_id);
            const auto& baseType = spirv.get_type(resource.base_type_id);

            const auto count = CalculateResourceCount(type);
            const auto dimension = GetResourceDimension(baseType.image.dim);

            auto image = ShaderResource(name, set, binding, EResourceType::SampledImage, dimension, count);
            result.push_back(std::move(image));
        }

        for (const auto& resource : resources.storage_images)
        {
            auto& name = spirv.get_name(resource.id);
            const auto set = spirv.get_decoration(resource.id, spv::DecorationDescriptorSet);
            const auto binding = spirv.get_decoration(resource.id, spv::DecorationBinding);

            const auto& type = spirv.get_type(resource.type_id);
            const auto& baseType = spirv.get_type(resource.base_type_id);

            const auto count = CalculateResourceCount(type);
            const auto dimension = GetResourceDimension(baseType.image.dim);

            auto image = ShaderResource(name, set, binding, EResourceType::StorageImage, dimension, count);
            result.push_back(std::move(image));
        }

        for (const auto& resource : resources.separate_samplers)
        {
            auto& name = spirv.get_name(resource.id);
            const auto set = spirv.get_decoration(resource.id, spv::DecorationDescriptorSet);
            const auto binding = spirv.get_decoration(resource.id, spv::DecorationBinding);

            const auto& type = spirv.get_type(resource.type_id);

            const auto count = CalculateResourceCount(type);

            auto image = ShaderResource(name, set, binding, EResourceType::Sampler, EResourceDimension::None, count);
            result.push_back(std::move(image));
        }

        return result;
    }

    std::vector<ShaderConstantBuffer> SpirVShaderBackend::GetConstantBuffers(
        const spirv_cross::Compiler& spirv,
        spirv_cross::ShaderResources& resources
    )
    {
        std::vector<ShaderConstantBuffer> result;

        for (const auto& resource : resources.uniform_buffers)
        {
            auto& name = spirv.get_name(resource.id);
            const auto set = spirv.get_decoration(resource.id, spv::DecorationDescriptorSet);
            const auto binding = spirv.get_decoration(resource.id, spv::DecorationBinding);

            const auto& baseType = spirv.get_type(resource.base_type_id);

            auto buffer = ShaderConstantBuffer(name, set, binding);

            // Loop through all members of the Uniform Buffer, and create a ShaderConstant
            // for each them.
            for (auto i = 0; i < baseType.member_types.size(); i++)
            {
                const auto& memberType = spirv.get_type(baseType.member_types[i]);
                const auto& memberName = spirv.get_member_name(baseType.self, i);

                auto type = EConstantType::Struct;
                Ref<ShaderConstantStruct> cstruct = nullptr;

                switch (memberType.basetype)
                {
                    case spirv_cross::SPIRType::Struct:
                        cstruct = ParseConstantStruct(spirv, memberType, memberName);
                        break;
                    case spirv_cross::SPIRType::Boolean:
                        type = EConstantType::Bool;
                        break;
                    case spirv_cross::SPIRType::Int:
                        type = EConstantType::Int;
                        break;
                    case spirv_cross::SPIRType::UInt:
                        type = EConstantType::Int;
                        break;
                    case spirv_cross::SPIRType::Float:
                        if (memberType.vecsize == 1)
                            type = EConstantType::Float;
                        else if (memberType.vecsize == 2)
                            type = EConstantType::Vec2;
                        else if (memberType.vecsize == 3)
                            type = EConstantType::Vec3;
                        else if (memberType.vecsize == 4 && memberType.columns == 1)
                            type = EConstantType::Vec4;
                        else if (memberType.vecsize == 4 && memberType.columns == 3)
                            type = EConstantType::Mat3;
                        else if (memberType.vecsize == 4 && memberType.columns == 4)
                            type = EConstantType::Mat4;
                        break;
                    default:
                        EE_CORE_ASSERT(false, "Unknown Constant Type!");
                }

                const auto count = CalculateResourceCount(memberType);

                ShaderConstant* constant;

                if (cstruct)
                {
                    constant = new ShaderConstant(memberName, std::move(cstruct), count, memberType.pointer);
                }
                else
                {
                    constant = new ShaderConstant(memberName, type, count, memberType.pointer);
                }

                buffer.PushConstant(std::move(*constant));
            }

            result.push_back(std::move(buffer));
        }

        return std::move(result);
    }

    std::vector<ShaderPushConstant> SpirVShaderBackend::GetPushConstants(
        const spirv_cross::Compiler& spirv,
        spirv_cross::ShaderResources& resources
    )
    {
        std::vector<ShaderPushConstant> result;

        for (const auto& resource : resources.push_constant_buffers)
        {
            auto& name = spirv.get_name(resource.id);
            const auto set = spirv.get_decoration(resource.id, spv::DecorationDescriptorSet);
            const auto binding = spirv.get_decoration(resource.id, spv::DecorationBinding);
            const auto offset = spirv.get_decoration(resource.id, spv::DecorationOffset);

            const auto& baseType = spirv.get_type(resource.base_type_id);

            auto buffer = ShaderPushConstant(name, set, binding, offset);

            // Loop through all members of the Push Constant Buffer, and create a ShaderConstant
            // for each them.
            for (auto i = 0; i < baseType.member_types.size(); i++)
            {
                const auto& memberType = spirv.get_type(baseType.member_types[i]);
                const auto& memberName = spirv.get_member_name(baseType.self, i);

                auto type = EConstantType::Struct;
                Ref<ShaderConstantStruct> cstruct = nullptr;

                switch (memberType.basetype)
                {
                    case spirv_cross::SPIRType::Struct:
                        cstruct = ParseConstantStruct(spirv, memberType, memberName);
                        break;
                    case spirv_cross::SPIRType::Boolean:
                        type = EConstantType::Bool;
                        break;
                    case spirv_cross::SPIRType::Int:
                        type = EConstantType::Int;
                        break;
                    case spirv_cross::SPIRType::UInt:
                        type = EConstantType::Int;
                        break;
                    case spirv_cross::SPIRType::Float:
                        if (memberType.vecsize == 1)
                            type = EConstantType::Float;
                        else if (memberType.vecsize == 2)
                            type = EConstantType::Vec2;
                        else if (memberType.vecsize == 3)
                            type = EConstantType::Vec3;
                        else if (memberType.vecsize == 4 && memberType.columns == 1)
                            type = EConstantType::Vec4;
                        else if (memberType.vecsize == 4 && memberType.columns == 3)
                            type = EConstantType::Mat3;
                        else if (memberType.vecsize == 4 && memberType.columns == 4)
                            type = EConstantType::Mat4;
                        break;
                    default:
                        EE_CORE_ASSERT(false, "Unknown Constant Type!");
                }

                const auto count = CalculateResourceCount(memberType);

                ShaderConstant* constant;

                if (cstruct)
                {
                    constant = new ShaderConstant(memberName, std::move(cstruct), count, memberType.pointer);
                }
                else
                {
                    constant = new ShaderConstant(memberName, type, count, memberType.pointer);
                }

                buffer.PushConstant(std::move(*constant));
            }

            result.push_back(std::move(buffer));
        }

        return std::move(result);
    }

    uint32_t SpirVShaderBackend::CalculateResourceCount(spirv_cross::SPIRType type)
    {
        auto count = 1;

        // Check if is an array and his size
        if (type.array.size() > 0)
        {
            count = 0;

            // TODO: Add matrices support, currently supports only vectors
            for (auto i = 0; i < type.array.size(); i++)
            {
                count += type.array[i];
            }
        }

        return count;
    }

    EResourceDimension SpirVShaderBackend::GetResourceDimension(const spv::Dim dimension)
    {
        switch (dimension)
        {
            case spv::Dim1D:    return EResourceDimension::_1D;
            case spv::Dim2D:    return EResourceDimension::_2D;
            case spv::Dim3D:    return EResourceDimension::_3D;
            case spv::DimCube:  return EResourceDimension::_3D;
            default:            return EResourceDimension::None;
        }
    }

    Scope<ShaderConstantStruct> SpirVShaderBackend::ParseConstantStruct(
        const spirv_cross::Compiler& spirv,
        spirv_cross::SPIRType type,
        const std::string& name
    )
    {
        auto cstruct = CreateScope<ShaderConstantStruct>(name);

        for (auto i = 0; i < type.member_types.size(); i++)
        {
            auto& fieldType = spirv.get_type(type.member_types[i]);
            auto& fieldName = spirv.get_member_name(type.self, i);

            auto type = EConstantType::Struct;
            Ref<ShaderConstantStruct> fieldStruct = nullptr;

            switch (fieldType.basetype)
            {
                case spirv_cross::SPIRType::Struct:
                    fieldStruct = ParseConstantStruct(spirv, fieldType, fieldName);
                    break;
                case spirv_cross::SPIRType::Boolean:
                    type = EConstantType::Bool;
                    break;
                case spirv_cross::SPIRType::Int:
                    type = EConstantType::Int;
                    break;
                case spirv_cross::SPIRType::UInt:
                    type = EConstantType::Int;
                    break;
                case spirv_cross::SPIRType::Float:
                    if (fieldType.vecsize == 1)
                        type = EConstantType::Float;
                    else if (fieldType.vecsize == 2)
                        type = EConstantType::Vec2;
                    else if (fieldType.vecsize == 3)
                        type = EConstantType::Vec3;
                    else if (fieldType.vecsize == 4 && fieldType.columns == 1)
                        type = EConstantType::Vec4;
                    else if (fieldType.vecsize == 4 && fieldType.columns == 3)
                        type = EConstantType::Mat3;
                    else if (fieldType.vecsize == 4 && fieldType.columns == 4)
                        type = EConstantType::Mat4;
                    break;
                default:
                    EE_CORE_ASSERT(false, "Unknown Constant Type!");
            }

            const auto count = CalculateResourceCount(fieldType);

            ShaderConstant* constant;

            if (cstruct)
            {
                constant = new ShaderConstant(fieldName, std::move(fieldStruct), count, fieldType.pointer);
            }
            else
            {
                constant = new ShaderConstant(fieldName, type, count, fieldType.pointer);
            }

            cstruct->AddField(std::move(*constant));
        }

        return cstruct;
    }
}