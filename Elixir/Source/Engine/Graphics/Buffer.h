#pragma once

#include <Engine/Graphics/GraphicsContext.h>

namespace Elixir
{
    enum class EDataType
    {
        Bool, Float, Vec2, Vec3, Vec4, Int, IntVec2, IntVec3, IntVec4, Mat3, Mat4
    };

    struct SBufferElement
    {
        SBufferElement(EDataType type, const std::string& name, bool normalized = false);

        uint32_t GetComponentCount() const;

        std::string Name;
        EDataType Type;
        size_t Offset;
        size_t Size;
        bool Normalized;
    };

    class BufferLayout
    {
      public:
        BufferLayout() : m_Stride(0) {}
        BufferLayout(const std::initializer_list<SBufferElement>& elements);

        [[nodiscard]] const std::vector<SBufferElement>& GetElements() const { return m_Elements; }
        [[nodiscard]] size_t GetStride() const { return m_Stride; }

        std::vector<SBufferElement>::iterator begin() { return m_Elements.begin(); }
		std::vector<SBufferElement>::iterator end() { return m_Elements.end(); }
		std::vector<SBufferElement>::const_iterator begin() const { return m_Elements.begin(); }
		std::vector<SBufferElement>::const_iterator end() const { return m_Elements.end(); }

      private:
        void CalculateOffsetsAndStride();

        std::vector<SBufferElement> m_Elements;
        size_t m_Stride;
    };

    typedef uint64_t BufferAddress;

    class ELIXIR_API GraphicsBuffer
    {
      public:
        virtual ~GraphicsBuffer() = default;

        [[nodiscard]] size_t GetSize() const { return m_Size; }

      protected:
        GraphicsBuffer(const GraphicsContext* context, size_t size);

        size_t m_Size;

        const GraphicsContext* m_GraphicsContext;
    };

    class ELIXIR_API VertexBuffer : virtual public GraphicsBuffer
    {
      public:
        ~VertexBuffer() override = default;

        [[nodiscard]] const BufferLayout& GetLayout() const { return m_Layout; }
        void SetLayout(const BufferLayout& layout) { m_Layout = layout; }

        [[nodiscard]] BufferAddress GetAddress() const { return m_Address; }

        static Ref<VertexBuffer> Create(const GraphicsContext* context, size_t size);
        static Ref<VertexBuffer> Create(
            const GraphicsContext* context,
            const void* data,
            size_t size
        );

      protected:
        VertexBuffer(const GraphicsContext* context, size_t size);

        BufferLayout m_Layout;
        BufferAddress m_Address;
    };

    enum class EIndexType
    {
        UInt16, UInt32
    };

    class ELIXIR_API IndexBuffer : virtual public GraphicsBuffer
    {
      public:
        ~IndexBuffer() override = default;

        [[nodiscard]] EIndexType GetIndexType() const { return m_IndexType; }

        static Ref<IndexBuffer> Create(
            const GraphicsContext* context,
            const void* data,
            size_t size = 0,
            EIndexType type = EIndexType::UInt32
        );

      protected:
        IndexBuffer(
            const GraphicsContext* context,
            size_t size,
            EIndexType type
        );

        EIndexType m_IndexType;
    };
}