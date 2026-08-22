#pragma once

#include <Engine.h>

// Base class for a pluggable editor UI panel (Hierarchy, Inspector, Viewport, ...).
// A panel owns a single root widget that EditorUI docks into the editor's content area.
class EditorPanel
{
public:
    virtual ~EditorPanel() = default;

    virtual const char* GetName() const = 0;

    // Build (or rebuild) this panel's widget tree and return its root widget.
    virtual Ref<GUI::Widget> Build() = 0;

    // Called once per frame, after the base Application has ticked its own GUI.
    virtual void OnUpdate(Timestep frameTime) {}
};
