// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "ui/view.h"

namespace cutefish::ui {

// FishUI's multi-line text area. Text lays out as Qt Quick's TextEdit lays
// it out; positions are UTF-8 byte offsets. Taller text than the area
// scrolls inside it. Read-only, the text can still be selected and copied.
class TextArea : public View
{
public:
    enum class WrapMode {
        NoWrap,
        // At word boundaries, as Text.WordWrap.
        WordWrap,
        // At any character, as Text.WrapAnywhere, for commands and paths.
        WrapAnywhere,
        // At word boundaries, inside a word that does not fit, as Text.Wrap.
        Wrap,
    };

    explicit TextArea(std::string text = {});
    ~TextArea() override;

    const std::string& text() const;
    // Moves the cursor to the end and clears the undo history.
    void setText(std::string text);
    void setPlaceholder(std::string placeholder);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;
    // WordWrap unless set, as FishUI's.
    void setWrapMode(WrapMode mode);
    // Around the text; 6 on every side unless set, as FishUI's.
    void setPadding(const Margins& padding);
    // FishUI's line under the text; off for an area inside a card.
    void setFrameVisible(bool visible);
    // Logical pixels; 0 follows the theme.
    void setFontSize(double size);

    size_t cursorPosition() const;
    void setCursorPosition(size_t position);
    void select(size_t anchor, size_t cursor);
    void selectAll();
    bool hasSelection() const;
    std::string selectedText() const;
    // What the input method is composing, shown at the cursor.
    const std::string& preedit() const;

    void cut();
    void copy();
    void paste();
    void undo();
    void redo();

    SizeF sizeHint() const override;
    double heightForWidth(double width) const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }
    CursorShape cursorShape() const override { return CursorShape::Text; }

    std::function<void()> onTextChanged;

protected:
    void paint(Painter& painter) override;
    void pointerPress(const PointerEvent& event) override;
    void pointerMove(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    void focusIn(FocusReason reason) override;
    void focusOut() override;
    bool keyPress(const KeyEvent& event) override;
    bool scroll(const ScrollEvent& event) override;
    bool acceptsTextInput() const override;
    TextInputState textInputState() const override;
    void textInput(const TextInputEvent& event) override;
    void layout() override;

private:
    struct Private;

    size_t offsetAt(PointF position) const;
    void showContextMenu(PointF position);
    void edited();
    void moved();

    std::unique_ptr<Private> d;
};

} // namespace cutefish::ui
