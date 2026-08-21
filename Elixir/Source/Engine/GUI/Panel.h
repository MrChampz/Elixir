#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class Manager;

    class ELIXIR_API Panel : public Widget
    {
    public:
        void Update(Timestep frameTime) override;

        /**
         * Remove all children from this panel, detaching each, and mark layout dirty.
         */
        void ClearChildren();

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding);

        /**
         * @brief Get the number of slots currently owned by this panel.
         * @return The number of slots currently owned by this panel.
         */
        virtual size_t GetSlotCount() const = 0;

        /**
         * @brief Get the slot at the specified index.
         * @param index The index of the slot to retrieve.
         * @return Non-owning pointer to the slot; valid until the next structural change
         * (AddChild/RemoveChild/ClearChildren) to this panel.
         */
        virtual Slot* GetSlotAt(size_t index) const = 0;

    protected:
        size_t GetChildCount() const override { return GetSlotCount(); }

        Ref<Widget> GetChildAt(size_t index) const override;

        // Remove a child widget from this panel: drops the slot holding it and clears the
        // child's parent back-pointer (via DetachChild), marking layout dirty.
        void RemoveChild(const Ref<Widget>& child) override;

        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

        // Erase the slot at index. Does not touch the child's parent back-pointer or mark
        // anything dirty - callers (RemoveChild) are responsible for that. Implemented by
        // TPanel<TSlot>, the only place that knows the concrete slot vector.
        virtual void RemoveSlotAt(size_t index) = 0;

        // Erase every slot. Does not touch any child's parent back-pointer or mark anything
        // dirty - callers (ClearChildren) are responsible for that. Implemented by
        // TPanel<TSlot>, the only place that knows the concrete slot vector.
        virtual void ClearSlots() = 0;

        SPadding m_Padding;
    };

    /**
     * Typed panel: the only place that constructs TSlot, so a container can never end up
     * holding the wrong slot type (e.g. a CanvasSlot inside a VerticalBox).
     * @tparam TSlot The type of slot this panel uses.
     */
    template <typename TSlot>
    class TPanel : public Panel
    {
    public:
        virtual TSlot& AddChild(const Ref<Widget>& child)
        {
            auto slot = CreateScope<TSlot>(child);
            TSlot& ref = *slot;

            m_Slots.push_back(std::move(slot));
            AttachChild(child);

            return ref;
        }

        size_t GetSlotCount() const override { return m_Slots.size(); }

        Slot* GetSlotAt(size_t index) const override { return m_Slots[index].get(); }

    protected:
        void RemoveSlotAt(const size_t index) override
        {
            m_Slots.erase(m_Slots.begin() + static_cast<std::ptrdiff_t>(index));
        }

        void ClearSlots() override
        {
            m_Slots.clear();
        }

        std::vector<Scope<TSlot>> m_Slots;
    };
}