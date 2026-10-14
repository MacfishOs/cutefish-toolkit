// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "ui/view.h"

namespace cutefish::ui {

// FishUI's single-line text field. Positions are UTF-8 byte offsets; input
// methods compose text in it while it has focus, and only then.
class TextField : public View
{
public:
    enum class EchoMode {
        Normal,
        // Dots in place of the text, which never reaches an input method or the clipboard.
        Password,
    };

    explicit TextField(std::string text = {});
    ~TextField() override;

    const std::string& text() const;
    // Moves the cursor to the end and clears the undo history.
    void setText(std::string text);
    void setPlaceholder(std::string placeholder);
    void setEchoMode(EchoMode mode);
    EchoMode echoMode() const;
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

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
    RectF visualRect() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }
    CursorShape cursorShape() const override { return CursorShape::Text; }

    std::function<void()> onTextChanged;
    // Return or Enter.
    std::function<void()> onAccepted;

protected:
    void paint(Painter& painter) override;
    void pointerPress(const PointerEvent& event) override;
    void pointerMove(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    void focusIn(FocusReason reason) override;
    void focusOut() override;
    bool keyPress(const KeyEvent& event) override;
    bool acceptsTextInput() const override;
    TextInputState textInputState() const override;
    void textInput(const TextInputEvent& event) override;

private:
    struct Private;

    void showContextMenu(PointF position);
    void edited();
    void moved();

    std::unique_ptr<Private> d;
};

} // namespace cutefish::ui
