#pragma once

namespace Elixir
{
    enum class EDataType : uint8_t
    {
        Bool, Float, Vec2, Vec3, Vec4, Int, IntVec2, IntVec3, IntVec4, Mat3, Mat4
    };

    enum class EInputRate : uint8_t
    {
        Vertex, Instance
    };

    class BufferAttribute
    {
        friend class BufferBinding;
      public:
        BufferAttribute(EDataType type, const std::string& name, bool normalized = false);

        uint32_t GetComponentCount() const;

        const std::string& GetName() const { return m_Name; }
        EDataType GetType() const { return m_Type; }
        size_t GetOffset() const { return m_Offset; }
        size_t GetSize() const { return m_Size; }
        bool IsNormalized() const { return m_Normalized; }

      private:
        std::string m_Name;
        EDataType m_Type;
        size_t m_Offset;
        size_t m_Size;
        bool m_Normalized;
    };

    class BufferBinding
    {
        friend class BufferLayout;
      public:
        BufferBinding() = default;
        BufferBinding(
            const std::initializer_list<BufferAttribute>& attributes,
            EInputRate inputRate = EInputRate::Vertex
        );

        uint32_t GetBinding() const { return m_Binding; }
        size_t GetStride() const { return m_Stride; }
        EInputRate GetInputRate() const { return m_InputRate; }
        const std::vector<BufferAttribute>& GetAttributes() const { return m_Attributes; }

        std::vector<BufferAttribute>::iterator begin() { return m_Attributes.begin(); }
        std::vector<BufferAttribute>::iterator end() { return m_Attributes.end(); }
        std::vector<BufferAttribute>::const_iterator begin() const { return m_Attributes.begin(); }
        std::vector<BufferAttribute>::const_iterator end() const { return m_Attributes.end(); }

      private:
        void CalculateOffsetsAndStride();

        uint32_t m_Binding = 0;
        size_t m_Stride = 0;
        EInputRate m_InputRate = EInputRate::Vertex;
        std::vector<BufferAttribute> m_Attributes;
    };

    class BufferLayout
    {
      public:
        BufferLayout() = default;
        BufferLayout(const std::initializer_list<BufferBinding>& bindings);

        void AddBinding(BufferBinding&& binding);

        const std::vector<BufferBinding>& GetBindings() const { return m_Bindings; }
        const BufferBinding* GetBinding(uint32_t binding) const;

        size_t GetBindingCount() const { return m_Bindings.size(); }
        size_t GetAttributeCount() const;

        std::vector<BufferBinding>::iterator begin() { return m_Bindings.begin(); }
        std::vector<BufferBinding>::iterator end() { return m_Bindings.end(); }
        std::vector<BufferBinding>::const_iterator begin() const { return m_Bindings.begin(); }
        std::vector<BufferBinding>::const_iterator end() const { return m_Bindings.end(); }

      private:
        std::vector<BufferBinding> m_Bindings;
    };
}