// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ui/event.h"
#include "ui/geometry.h"

class SkRegion;

namespace cutefish::ui {

struct Layer;
class Painter;
class Window;

enum class Alignment {
    // The layout's own: start in a column, centre in a row.
    Default,
    Start,
    Center,
    End,
};

// How a layout places a view. Lengths are logical pixels.
struct LayoutHints
{
    bool fillWidth = false;
    bool fillHeight = false;
    // The view's share of the space left over, against its filling siblings.
    double stretch = 1;
    // Across the layout's direction.
    Alignment alignment = Alignment::Default;
    // Negative: from the size hint.
    double preferredWidth = -1;
    double preferredHeight = -1;
};

// A node of a window's retained tree. Geometry is in logical pixels,
// relative to the parent; the painter puts edges on device pixels.
class View
{
public:
    View();
    virtual ~View();

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    // For tests, layout dumps and accessibility; not shown.
    const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    View* parent() const { return m_parent; }
    Window* window() const;

    template<typename T, typename... Args>
    T* addChild(Args&&... args)
    {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* view = child.get();
        addChild(std::unique_ptr<View>(std::move(child)));
        return view;
    }
    void addChild(std::unique_ptr<View> child);
    std::unique_ptr<View> takeChild(View* child);
    // Moves the child above its siblings.
    void raiseChild(View* child);
    const std::vector<std::unique_ptr<View>>& children() const { return m_children; }

    const RectF& geometry() const { return m_geometry; }
    void setGeometry(const RectF& geometry);
    RectF localRect() const { return {0, 0, m_geometry.width, m_geometry.height}; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);

    // Children paint only inside this view.
    bool clipsChildren() const { return m_clipsChildren; }
    void setClipsChildren(bool clips);

    // Applies to the view and its children.
    double opacity() const { return m_opacity; }
    void setOpacity(double opacity);

    // Applied as the view and its children are painted and hit; layout
    // ignores it. Anything but the identity makes the view layered.
    const Transform& transform() const { return m_transform; }
    void setTransform(const Transform& transform);
    // Paints the view and its children into a texture of their own, kept
    // until they change, so opacity and transform changes cost no repaint.
    // Opacity then applies to them as one, and text loses subpixel colour.
    bool isLayered() const { return m_layered; }
    void setLayered(bool layered);

    // Everything the view paints, in its own coordinates: its rect, and more
    // where shadows or focus rings reach out.
    virtual RectF visualRect() const { return localRect(); }

    // A disabled view and its children take no input or focus.
    bool isEnabled() const;
    void setEnabled(bool enabled);

    // Keyboard focus, which only views that accept it can take.
    virtual bool acceptsFocus() const { return false; }
    bool hasFocus() const;
    // Focus that came from the keyboard, which controls draw.
    bool hasVisibleFocus() const;
    void setFocus(FocusReason reason = FocusReason::Other);

    virtual CursorShape cursorShape() const { return CursorShape::Default; }

    // Tells the input method that textInputState() changed.
    void updateTextInput();

    // Shown over the view after the pointer rests on it.
    const std::string& toolTip() const { return m_toolTip; }
    void setToolTip(std::string text) { m_toolTip = std::move(text); }

    const LayoutHints& layoutHints() const { return m_layoutHints; }
    void setLayoutHints(const LayoutHints& hints);

    virtual SizeF sizeHint() const { return {}; }
    // For views whose height follows their width, such as wrapped text.
    virtual double heightForWidth(double) const { return sizeHint().height; }
    // The size hint, overridden by the preferred sizes in the layout hints.
    SizeF preferredSize() const;

    void requestLayout();
    void requestPaint();

    PointF mapFromWindow(PointF point) const;
    PointF mapToWindow(PointF point) const;

    // Only views that accept the pointer receive pointer input; a press goes
    // to the topmost one under the pointer, which keeps it until the release.
    virtual bool acceptsPointer() const { return false; }

protected:
    virtual void pointerEnter() {}
    virtual void pointerLeave() {}
    virtual void pointerPress(const PointerEvent&) {}
    virtual void pointerMove(const PointerEvent&) {}
    virtual void pointerRelease(const PointerEvent&) {}

    virtual void focusIn(FocusReason) {}
    virtual void focusOut() {}
    // Return true when handled; the parent gets what the view leaves.
    virtual bool keyPress(const KeyEvent&) { return false; }
    virtual bool keyRelease(const KeyEvent&) { return false; }
    // To the view under the pointer, then up through its parents.
    virtual bool scroll(const ScrollEvent&) { return false; }

    // Views that edit text take input methods while they have focus.
    virtual bool acceptsTextInput() const { return false; }
    virtual TextInputState textInputState() const { return {}; }
    virtual void textInput(const TextInputEvent&) {}

    // Positions the children once this view has its geometry.
    virtual void layout() {}
    virtual void paint(Painter&) {}

private:
    friend class TestInput;
    friend class Window;

    void layoutTree();
    // Culls what lies outside the region, in the target's device pixels.
    void paintTree(Painter& painter, const SkRegion* region);
    // The view and its children as painted into its layer.
    void paintContent(Painter& painter, const SkRegion* region);
    bool needsLayer() const { return m_layered || !m_transform.isIdentity(); }
    // Drawn from its texture: as last painted, or as it will be.
    bool isComposited(bool painted) const { return painted ? m_layer != nullptr : needsLayer(); }
    // Before a change: damages where the view and its children are now,
    // once until the next frame, which damages where they are then.
    void invalidate();
    // As invalidate(), but keeps a layer's texture: its opacity or transform
    // changed, not what it shows.
    void invalidateComposite();
    // The topmost visible view at the point; with input, only enabled views
    // that accept the pointer.
    View* viewAt(PointF point, bool input);
    bool contains(const View* view) const;
    bool isVisibleInTree() const;

    std::string m_name;
    std::string m_toolTip;
    View* m_parent = nullptr;
    Window* m_window = nullptr;
    std::vector<std::unique_ptr<View>> m_children;
    RectF m_geometry;
    LayoutHints m_layoutHints;
    bool m_visible = true;
    bool m_enabled = true;
    bool m_clipsChildren = false;
    double m_opacity = 1;
    Transform m_transform;
    bool m_layered = false;
    std::unique_ptr<Layer> m_layer;
    bool m_dirty = false;
    bool m_compositeDirty = false;
};

} // namespace cutefish::ui
