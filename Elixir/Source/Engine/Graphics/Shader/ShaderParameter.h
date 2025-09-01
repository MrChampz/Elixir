#pragma once

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
      public:
        [[nodiscard]] const std::string& GetName() const { return m_Name; }
		[[nodiscard]] uint32_t GetSet() const { return m_Set; }
		[[nodiscard]] uint32_t GetBinding() const { return m_Binding; }
		[[nodiscard]] EResourceType GetType() const { return m_Type; }
		[[nodiscard]] EResourceDimension GetDimension() const { return m_Dimension; }
		[[nodiscard]] uint32_t GetCount() const { return m_Count; }

      protected:
        ShaderResource(
            const std::string& name,
            uint32_t set,
            uint32_t binding,
            EResourceType type,
            EResourceDimension dimension,
            uint32_t count
        );

        std::string m_Name;
        uint32_t m_Set;
        uint32_t m_Binding;
        EResourceType m_Type;
        EResourceDimension m_Dimension;
        uint32_t m_Count;
    };

    enum class EConstantType : uint8_t
    {
        Struct = 0,
        Bool,
        Float, Vec2, Vec3, Vec4,
        Int, IntVec2, IntVec3, IntVec4,
        Mat3, Mat4
    };

    class ShaderConstantStruct;

    class ShaderConstant final
    {
        friend class ShaderConstantStruct;
        friend class ShaderConstantBuffer;
        friend class ShaderPushConstant;

      public:
        [[nodiscard]] std::string GetName() const { return m_Name; }
        [[nodiscard]] EConstantType GetType() const { return m_Type; }
        [[nodiscard]] uint32_t GetCount() const { return m_Count; }
        [[nodiscard]] bool IsPointer() const { return m_Pointer; }
        [[nodiscard]] bool IsArray() const { return m_Count > 1; }
        [[nodiscard]] uint32_t GetSize() const { return m_Size; }
        [[nodiscard]] uint32_t GetOffset() const { return m_Offset; }

      protected:
        ShaderConstant(
            const std::string& name,
            EConstantType type,
            uint32_t count,
            bool pointer
        );

        ShaderConstant(
            const std::string& name,
            Scope<ShaderConstantStruct> structure,
            uint32_t count,
            bool pointer
        );

        void SetOffset(uint32_t offset);

      private:
        std::string m_Name;
        EConstantType m_Type;
        Scope<ShaderConstantStruct> m_Struct;
        uint32_t m_Count = 0;
        bool m_Pointer = false;

        uint32_t m_Size = 0;
        uint32_t m_Offset = 0;
    };

    class ShaderConstantStruct final
    {
        friend class ShaderConstant;

      public:
        ~ShaderConstantStruct();

        [[nodiscard]] std::string GetName() const { return m_Name; }
        [[nodiscard]] uint32_t GetSize() const { return m_Size; }
        [[nodiscard]] uint32_t GetOffset() const { return m_Offset; }
        [[nodiscard]] const std::vector<ShaderConstant>& GetFields() const { return m_Fields; }

      protected:
        explicit ShaderConstantStruct(const std::string& name)
            : m_Name(name) {}

        void AddField(ShaderConstant* field);

        void SetOffset(const uint32_t offset) { m_Offset = offset; }

        std::string m_Name;
        std::vector<ShaderConstant> m_Fields;

        uint32_t m_Size = 0;
        uint32_t m_Offset = 0;
    };

    class ShaderConstantBuffer final
    {
      public:
        ~ShaderConstantBuffer();

        [[nodiscard]] const std::string& GetName() const { return m_Name; }
        [[nodiscard]] uint32_t GetSet() const { return m_Set; }
        [[nodiscard]] uint32_t GetBinding() const { return m_Binding; }
        [[nodiscard]] uint32_t GetSize() const { return m_Size; }
        [[nodiscard]] const std::vector<ShaderConstant>& GetConstants() const { return m_Constants; }

      protected:
        ShaderConstantBuffer(const std::string& name, uint32_t set, uint32_t binding);

        void PushConstant(ShaderConstant* constant);
        [[nodiscard]] ShaderConstant* FindConstant(const std::string& name);

        std::string m_Name;
        uint32_t m_Set = 0;
        uint32_t m_Binding = 0;
        std::vector<ShaderConstant> m_Constants;

        uint32_t m_Size = 0;
    };

    class ShaderPushConstant final
    {
      public:
        ~ShaderPushConstant();

        [[nodiscard]] const std::string& GetName() const { return m_Name; }
		[[nodiscard]] uint32_t GetSet() const { return m_Set; }
		[[nodiscard]] uint32_t GetBinding() const { return m_Binding; }
		[[nodiscard]] uint32_t GetSize() const { return m_Size; }
		[[nodiscard]] uint32_t GetOffset() const { return m_Offset; }
		[[nodiscard]] const std::vector<ShaderConstant>& GetConstants() const { return m_Constants; }
		
      protected:
        ShaderPushConstant(
            const std::string& name,
            uint32_t set,
			uint32_t binding,
			uint32_t offset
        );

        void PushConstant(ShaderConstant* constant);
        [[nodiscard]] ShaderConstant* FindConstant(const std::string& name);
        
        std::string m_Name;
        uint32_t m_Set = 0;
        uint32_t m_Binding = 0;
        std::vector<ShaderConstant> m_Constants;

        uint32_t m_Size = 0;
        uint32_t m_Offset = 0;
    };
}