#pragma once

#include <Engine/Graphics/GraphicsTypes.h>

namespace Elixir
{
    constexpr uint32_t POINTER_SIZE = 8;

    enum class EResourceType : uint8_t
    {
        Image = 0, SampledImage, StorageImage, Sampler
    };

    enum class EResourceDimension : uint8_t
    {
        None = 0, _1D, _2D, _3D
    };

    class ShaderResource final
    {
        friend class Shader;
      public:
        ShaderResource(
            const std::string& name,
            uint32_t set,
            uint32_t binding,
            EResourceType type,
            EResourceDimension dimension,
            uint32_t count,
            bool bindless = false
        );

        ShaderResource(const ShaderResource&) = default;
        ShaderResource(ShaderResource&&) noexcept = default;

        const std::string& GetName() const { return m_Name; }
		uint32_t GetSet() const { return m_Set; }
		uint32_t GetBinding() const { return m_Binding; }
		EResourceType GetType() const { return m_Type; }
		EResourceDimension GetDimension() const { return m_Dimension; }
		uint32_t GetCount() const { return m_Count; }
        EShaderStage GetShaderStages() const { return m_Stages; }
        bool IsBindless() const { return m_IsBindless; }

        ShaderResource& operator=(const ShaderResource&) = default;
        ShaderResource& operator=(ShaderResource&&) noexcept = default;

      protected:
        std::string m_Name;
        uint32_t m_Set;
        uint32_t m_Binding;
        EResourceType m_Type;
        EResourceDimension m_Dimension;
        uint32_t m_Count;
        EShaderStage m_Stages = EShaderStage::None;
        bool m_IsBindless;
    };

    enum class EConstantType : uint8_t
    {
        Struct = 0,
        Bool,
        Float, Vec2, Vec3, Vec4,
        Int, IntVec2, IntVec3, IntVec4,
        UInt, UIntVec2, UIntVec3, UIntVec4,
        Mat3, Mat4
    };

    class ShaderConstantStruct;

    class ShaderConstant final
    {
        friend class ShaderConstantStruct;
        friend class ShaderStorageBuffer;
        friend class ShaderConstantBuffer;
        friend class ShaderPushConstant;

      public:
        ShaderConstant(
            const std::string& name,
            EConstantType type,
            uint32_t count,
            bool pointer
        );

        ShaderConstant(
            const std::string& name,
            Ref<ShaderConstantStruct> structure,
            uint32_t count,
            bool pointer
        );

        ShaderConstant(const ShaderConstant&) = default;
        ShaderConstant(ShaderConstant&&) noexcept = default;

        std::string GetName() const { return m_Name; }
        EConstantType GetType() const { return m_Type; }
        uint32_t GetCount() const { return m_Count; }
        bool IsPointer() const { return m_Pointer; }
        bool IsArray() const { return m_Count > 1; }
        uint32_t GetSize() const { return m_Size; }
        uint32_t GetOffset() const { return m_Offset; }

        ShaderConstant& operator=(const ShaderConstant&) = default;
        ShaderConstant& operator=(ShaderConstant&&) noexcept = default;

      protected:
        void SetOffset(uint32_t offset);

      private:
        std::string m_Name;
        EConstantType m_Type;
        Ref<ShaderConstantStruct> m_Struct;
        uint32_t m_Count = 0;
        bool m_Pointer = false;

        uint32_t m_Size = 0;
        uint32_t m_Offset = 0;
    };

    class ShaderConstantStruct final
    {
        friend class ShaderConstant;

      public:
        explicit ShaderConstantStruct(const std::string& name) : m_Name(name) {}

        void AddField(ShaderConstant&& field);

        std::string GetName() const { return m_Name; }
        uint32_t GetSize() const { return m_Size; }
        uint32_t GetOffset() const { return m_Offset; }
        const std::vector<ShaderConstant>& GetFields() const { return m_Fields; }

      protected:
        ShaderConstantStruct(const ShaderConstantStruct&) = delete;
        ShaderConstantStruct& operator=(const ShaderConstantStruct&) = delete;

        void SetOffset(const uint32_t offset) { m_Offset = offset; }

        std::string m_Name;
        std::vector<ShaderConstant> m_Fields;

        uint32_t m_Size = 0;
        uint32_t m_Offset = 0;
    };

    class ELIXIR_API ShaderStorageBuffer final
    {
        friend class Shader;
    public:
        ShaderStorageBuffer(const std::string& name, uint32_t set, uint32_t binding);
        ShaderStorageBuffer(const ShaderStorageBuffer&) = default;
        ShaderStorageBuffer(ShaderStorageBuffer&&) noexcept = default;

        void PushConstant(ShaderConstant&& constant);
        const ShaderConstant* FindConstant(const std::string& name);

        const std::string& GetName() const { return m_Name; }
        uint32_t GetSet() const { return m_Set; }
        uint32_t GetBinding() const { return m_Binding; }
        uint32_t GetSize() const { return m_Size; }
        const std::vector<ShaderConstant>& GetConstants() const { return m_Constants; }
        EShaderStage GetShaderStages() const { return m_Stages; }

        ShaderStorageBuffer& operator=(const ShaderStorageBuffer&) = default;

    protected:
        std::string m_Name;
        uint32_t m_Set = 0;
        uint32_t m_Binding = 0;
        std::vector<ShaderConstant> m_Constants;
        EShaderStage m_Stages = EShaderStage::None;

        uint32_t m_Size = 0;
    };

    class ELIXIR_API ShaderConstantBuffer final
    {
        friend class Shader;
      public:
        ShaderConstantBuffer(const std::string& name, uint32_t set, uint32_t binding);

        ShaderConstantBuffer(const ShaderConstantBuffer&) = default;
        ShaderConstantBuffer(ShaderConstantBuffer&&) noexcept = default;

        void PushConstant(ShaderConstant&& constant);
        const ShaderConstant* FindConstant(const std::string& name);

        const std::string& GetName() const { return m_Name; }
        uint32_t GetSet() const { return m_Set; }
        uint32_t GetBinding() const { return m_Binding; }
        uint32_t GetSize() const { return m_Size; }
        const std::vector<ShaderConstant>& GetConstants() const { return m_Constants; }
        EShaderStage GetShaderStages() const { return m_Stages; }

        ShaderConstantBuffer& operator=(const ShaderConstantBuffer&) = default;

      protected:
        std::string m_Name;
        uint32_t m_Set = 0;
        uint32_t m_Binding = 0;
        std::vector<ShaderConstant> m_Constants;
        EShaderStage m_Stages = EShaderStage::None;

        uint32_t m_Size = 0;
    };

    class ELIXIR_API ShaderPushConstant final
    {
        friend class Shader;
      public:
        ShaderPushConstant(
            const std::string& name,
            uint32_t set,
            uint32_t binding,
            uint32_t offset
        );

        ShaderPushConstant(const ShaderPushConstant&) noexcept = default;
        ShaderPushConstant(ShaderPushConstant&&) noexcept = default;

        void PushConstant(ShaderConstant&& constant);
        const ShaderConstant* FindConstant(const std::string& name);

        const std::string& GetName() const { return m_Name; }
		uint32_t GetSet() const { return m_Set; }
		uint32_t GetBinding() const { return m_Binding; }
		uint32_t GetSize() const { return m_Size; }
		uint32_t GetOffset() const { return m_Offset; }
		const std::vector<ShaderConstant>& GetConstants() const { return m_Constants; }
        EShaderStage GetShaderStages() const { return m_Stages; }

        ShaderPushConstant& operator=(const ShaderPushConstant&) = default;
        ShaderPushConstant& operator=(ShaderPushConstant&&) noexcept = default;

      protected:
        std::string m_Name;
        uint32_t m_Set = 0;
        uint32_t m_Binding = 0;
        std::vector<ShaderConstant> m_Constants;
        EShaderStage m_Stages = EShaderStage::None;

        uint32_t m_Size = 0;
        uint32_t m_Offset = 0;
    };
}