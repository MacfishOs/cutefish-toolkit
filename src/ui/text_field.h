// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
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
    // TextInput.maximumLength, in UTF-16 units as Qt counts; 0 for none.
    // Typing and pasting stop at it.
    void setMaximumLength(size_t length);
    // An edit the validator refuses does not happen, as with a QValidator
    // that finds the text Invalid.
    void setValidator(std::function<bool(const std::string& text)> validator);
    // Logical pixels; 0 follows the theme.
    void setFontSize(double size);
    // Around the text, left and right; FishUI's large spacing unless set.
    void setTextMargins(double left, double right);
    // The rounded background and its border; off for a field drawn into
    // something else, as a background: null TextField.
    void setFrameVisible(bool visible);
    // Qt Quick's bare TextInput, with the frame off and no margins:
    // FishUI fades a disabled field to half; a TextInput does not.
    void setFadesWhenDisabled(bool fades);
    // The text and the caret, in every state; unset follows the theme.
    void setTextColor(std::optional<Color> color);
    void setPlaceholderColor(std::optional<Color> color);
    // TextInput's selectionColor and selectedTextColor; unset, the theme's
    // highlight colours.
    void setSelectionColors(std::optional<Color> background, std::optional<Color> text);
    // A family as Label::setFontFamily takes it; empty follows the theme.
    void setFontFamily(std::string family);
    // What a password shows for each character; U+2022 unless set.
    void setPasswordCharacter(std::string character);
    // Logical; drawn as whole device pixels, at least one. Unset, one device
    // pixel per whole scale.
    void setCaretWidth(double width);
    // QStyleHints::cursorFlashTime: the caret is shown and hidden for half
    // of it each, as Qt's coarse timer runs it (5% early); 1000 unless set.
    void setCursorFlashTime(int milliseconds);
    // TextInput.selectByMouse: off, a press only moves the cursor.
    void setSelectByMouse(bool select);
    // Off, copy, cut and paste do nothing, from keys or the menu.
    void setClipboardEnabled(bool enabled);
    // The menu on a right press; embedded windows never show one.
    void setContextMenuEnabled(bool enabled);
    // Empties the field, as TextInput.clear().
    void clear();

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

    // Any change of the text, from the user or from code.
    std::function<void()> onTextChanged;
    // Changes the user made, as textEdited.
    std::function<void()> onTextEdited;
    // Return or Enter.
    std::function<void()> onAccepted;
    // Return, Enter or losing the focus, as editingFinished.
    std::function<void()> onEditingFinished;

protected:
    void paint(Painter& painter) override;
    void pointerPress(const PointerEvent& event) override;
    void pointerMove(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    void focusIn(FocusReason reason) override;
    void focusOut() override;
    void enabledChanged() override;
    bool keyPress(const KeyEvent& event) override;
    bool acceptsTextInput() const override;
    TextInputState textInputState() const override;
    void textInput(const TextInputEvent& event) override;
    // Before onTextChanged.
    virtual void textChanged() {}

private:
    struct Private;

    void showContextMenu(PointF position);
    // After the user's edit; false when it was refused and undone.
    bool edited();
    void moved();

    std::unique_ptr<Private> d;
};

} // namespace cutefish::ui
