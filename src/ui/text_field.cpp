// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/text_field.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"

#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/clipboard.h"
#include "ui/editable_text.h"
#include "ui/keys.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/text_context.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace textlayout = skia::textlayout;

namespace cutefish::ui {

static constexpr double kMinimumWidth = 200;
// FishUI's: a medium icon and half the small spacing.
static constexpr double kHeight = 35;
// FishUI's focus ring, outside the field.
static constexpr double kFocusRing = 2;
static constexpr auto kBlinkInterval = std::chrono::milliseconds(500);
// Wide enough that a single line never wraps.
static constexpr double kUnboundedWidth = 1e6;
// U+2022 BULLET.
static constexpr const char* kPasswordDot = "•";
static constexpr size_t kPasswordDotSize = 3;
// The surrounding text an input method gets around the cursor.
static constexpr size_t kSurroundingLimit = 4000;

// UTF-16 code units before the byte offset, as the paragraph counts.
static size_t utf16Index(const std::string& text, size_t offset)
{
    size_t index = 0;
    for (size_t i = 0; i < offset && i < text.size(); ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if ((byte & 0xc0) != 0x80)
            index += byte >= 0xf0 ? 2 : 1;
    }
    return index;
}

static size_t utf8Offset(const std::string& text, size_t index)
{
    size_t units = 0;
    size_t i = 0;
    for (; i < text.size(); ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if ((byte & 0xc0) == 0x80)
            continue;
        if (units >= index)
            break;
        units += byte >= 0xf0 ? 2 : 1;
    }
    return i;
}

struct TextField::Private
{
    EditableText edit;
    std::string placeholder;
    EchoMode echo = EchoMode::Normal;
    bool readOnly = false;
    std::string preedit;
    int preeditCursor = -1;

    std::unique_ptr<textlayout::Paragraph> paragraph;
    std::string display;
    double paragraphScale = 0;
    bool paragraphEnabled = true;
    uint64_t paragraphTheme = 0;
    bool dirty = true;
    std::unique_ptr<textlayout::Paragraph> placeholderParagraph;
    double placeholderScale = 0;
    uint64_t placeholderTheme = 0;

    // Device pixels the text is scrolled by to keep the cursor in sight.
    double scrollX = 0;
    bool caretVisible = true;
    int blinkTimer = 0;
    bool selecting = false;
    int granularity = 1;
    std::pair<size_t, size_t> anchorWord;
    std::shared_ptr<TextField*> self;
    std::unique_ptr<Menu> menu;

    // The text as shown: the preedit inside it at the cursor, or dots.
    std::string displayText() const
    {
        if (echo == EchoMode::Password) {
            std::string dots;
            for (size_t at = 0; at < edit.text().size(); at = edit.nextGrapheme(at))
                dots += kPasswordDot;
            return dots;
        }
        if (preedit.empty())
            return edit.text();
        std::string text = edit.text();
        text.insert(edit.cursor(), preedit);
        return text;
    }

    size_t toDisplay(size_t offset) const
    {
        if (echo == EchoMode::Password) {
            size_t graphemes = 0;
            for (size_t at = 0; at < offset; at = edit.nextGrapheme(at))
                ++graphemes;
            return graphemes * kPasswordDotSize;
        }
        return offset <= edit.cursor() ? offset : offset + preedit.size();
    }

    size_t fromDisplay(size_t offset) const
    {
        if (echo == EchoMode::Password) {
            size_t at = 0;
            for (size_t dots = offset / kPasswordDotSize; dots > 0 && at < edit.text().size(); --dots)
                at = edit.nextGrapheme(at);
            return at;
        }
        const size_t cursor = edit.cursor();
        if (offset <= cursor)
            return offset;
        if (offset >= cursor + preedit.size())
            return offset - preedit.size();
        return cursor;
    }

    // Where the caret shows, in the display text.
    size_t caretDisplayOffset() const
    {
        const size_t cursor = toDisplay(edit.cursor());
        if (!preedit.empty() && preeditCursor >= 0)
            return cursor + std::min(size_t(preeditCursor), preedit.size());
        return cursor;
    }

    textlayout::Paragraph* layout(double scale, bool enabled)
    {
        if (paragraph && !dirty && paragraphScale == scale && paragraphEnabled == enabled
            && paragraphTheme == Theme::generation())
            return paragraph.get();

        const TextContext& context = Application::instance()->text();
        const Theme& theme = Theme::current();
        const Color color = enabled ? theme.text : theme.disabledText;
        const textlayout::TextStyle normal = context.style(0, scale, 400, color);
        textlayout::ParagraphStyle style;
        style.setTextStyle(normal);
        style.setMaxLines(1);

        display = displayText();
        auto builder = textlayout::ParagraphBuilder::make(style, context.fontCollection(),
                                                          context.unicode());
        builder->pushStyle(normal);
        const auto add = [&builder, this](size_t start, size_t end) {
            if (end > start)
                builder->addText(display.data() + start, end - start);
        };
        if (!preedit.empty()) {
            const size_t start = edit.cursor();
            textlayout::TextStyle underlined = normal;
            underlined.setDecoration(textlayout::TextDecoration::kUnderline);
            underlined.setDecorationColor(color.argb());
            add(0, start);
            builder->pushStyle(underlined);
            add(start, start + preedit.size());
            builder->pop();
            add(start + preedit.size(), display.size());
        } else if (edit.hasSelection()) {
            const size_t start = toDisplay(edit.selectionStart());
            const size_t end = toDisplay(edit.selectionEnd());
            textlayout::TextStyle selected = normal;
            selected.setColor(theme.highlightedText.argb());
            add(0, start);
            builder->pushStyle(selected);
            add(start, end);
            builder->pop();
            add(end, display.size());
        } else {
            add(0, display.size());
        }
        builder->pop();
        paragraph = builder->Build();
        paragraph->layout(SkScalar(kUnboundedWidth));
        paragraphScale = scale;
        paragraphEnabled = enabled;
        paragraphTheme = Theme::generation();
        dirty = false;
        return paragraph.get();
    }

    textlayout::Paragraph* placeholderLayout(double scale)
    {
        if (placeholderParagraph && placeholderScale == scale
            && placeholderTheme == Theme::generation())
            return placeholderParagraph.get();
        const TextContext& context = Application::instance()->text();
        const textlayout::TextStyle style = context.style(0, scale, 400,
                                                          Theme::current().text.withAlpha(0.4));
        textlayout::ParagraphStyle paragraphStyle;
        paragraphStyle.setTextStyle(style);
        paragraphStyle.setMaxLines(1);
        paragraphStyle.setEllipsis(SkString("…"));
        auto builder = textlayout::ParagraphBuilder::make(paragraphStyle, context.fontCollection(),
                                                          context.unicode());
        builder->pushStyle(style);
        builder->addText(placeholder.data(), placeholder.size());
        builder->pop();
        placeholderParagraph = builder->Build();
        placeholderScale = scale;
        placeholderTheme = Theme::generation();
        return placeholderParagraph.get();
    }

    // Device pixels from the start of the text to the caret at a display offset.
    double caretX(size_t offset) const
    {
        if (display.empty() || !paragraph)
            return 0;
        const size_t index = utf16Index(display, offset);
        const size_t length = utf16Index(display, display.size());
        textlayout::Paragraph::GlyphInfo info;
        if (index < length && paragraph->getGlyphInfoAtUTF16Offset(index, &info)) {
            return info.fDirection == textlayout::TextDirection::kRtl
                    ? info.fGraphemeLayoutBounds.right()
                    : info.fGraphemeLayoutBounds.left();
        }
        if (index > 0 && paragraph->getGlyphInfoAtUTF16Offset(index - 1, &info)) {
            return info.fDirection == textlayout::TextDirection::kRtl
                    ? info.fGraphemeLayoutBounds.left()
                    : info.fGraphemeLayoutBounds.right();
        }
        return 0;
    }

    // The display offset nearest to a device x from the start of the text.
    size_t displayOffsetAt(double x) const
    {
        if (display.empty() || !paragraph)
            return 0;
        textlayout::Paragraph::GlyphInfo info;
        if (!paragraph->getClosestUTF16GlyphInfoAt(SkScalar(x), paragraph->getHeight() / 2, &info))
            return 0;
        const SkRect& bounds = info.fGraphemeLayoutBounds;
        bool before = x < bounds.centerX();
        if (info.fDirection == textlayout::TextDirection::kRtl)
            before = !before;
        const size_t index = before ? info.fGraphemeClusterTextRange.start
                                    : info.fGraphemeClusterTextRange.end;
        return utf8Offset(display, index);
    }
};

TextField::TextField(std::string text)
    : d(std::make_unique<Private>())
{
    d->self = std::make_shared<TextField*>(this);
    d->edit.setText(std::move(text));
}

TextField::~TextField()
{
    if (d->blinkTimer)
        Application::instance()->loop().stopTimer(d->blinkTimer);
}

const std::string& TextField::text() const
{
    return d->edit.text();
}

void TextField::setText(std::string text)
{
    if (text == d->edit.text())
        return;
    d->edit.setText(std::move(text));
    d->preedit.clear();
    edited();
}

void TextField::setPlaceholder(std::string placeholder)
{
    d->placeholder = std::move(placeholder);
    d->placeholderParagraph.reset();
    requestPaint();
}

void TextField::setEchoMode(EchoMode mode)
{
    d->echo = mode;
    d->dirty = true;
    updateTextInput();
    requestPaint();
}

TextField::EchoMode TextField::echoMode() const
{
    return d->echo;
}

void TextField::setReadOnly(bool readOnly)
{
    d->readOnly = readOnly;
    updateTextInput();
}

bool TextField::isReadOnly() const
{
    return d->readOnly;
}

size_t TextField::cursorPosition() const
{
    return d->edit.cursor();
}

void TextField::setCursorPosition(size_t position)
{
    d->edit.setCursor(position);
    moved();
}

void TextField::select(size_t anchor, size_t cursor)
{
    d->edit.select(anchor, cursor);
    moved();
}

void TextField::selectAll()
{
    d->edit.selectAll();
    moved();
}

bool TextField::hasSelection() const
{
    return d->edit.hasSelection();
}

std::string TextField::selectedText() const
{
    return d->edit.selectedText();
}

const std::string& TextField::preedit() const
{
    return d->preedit;
}

void TextField::cut()
{
    if (d->readOnly || d->echo == EchoMode::Password || !d->edit.hasSelection())
        return;
    copy();
    d->edit.deleteBackward(false);
    edited();
}

void TextField::copy()
{
    if (d->echo == EchoMode::Password || !d->edit.hasSelection())
        return;
    Clipboard::setText(d->edit.selectedText());
}

void TextField::paste()
{
    if (d->readOnly)
        return;
    Clipboard::requestText([self = std::weak_ptr<TextField*>(d->self)](std::string text) {
        const auto field = self.lock();
        if (!field || text.empty())
            return;
        // One line: line breaks become spaces.
        std::replace(text.begin(), text.end(), '\n', ' ');
        std::erase(text, '\r');
        (*field)->d->edit.insert(text);
        (*field)->edited();
    });
}

void TextField::undo()
{
    if (d->readOnly || !d->edit.canUndo())
        return;
    d->edit.undo();
    edited();
}

void TextField::redo()
{
    if (d->readOnly || !d->edit.canRedo())
        return;
    d->edit.redo();
    edited();
}

RectF TextField::visualRect() const
{
    return localRect().adjusted(-kFocusRing, -kFocusRing, kFocusRing, kFocusRing);
}

SizeF TextField::sizeHint() const
{
    double width = kMinimumWidth;
    if (!d->placeholder.empty()) {
        const double scale = window() ? window()->scale() : 1.0;
        textlayout::Paragraph* placeholder = d->placeholderLayout(scale);
        placeholder->layout(SkScalar(kUnboundedWidth));
        width = std::max(width, std::ceil(placeholder->getMaxIntrinsicWidth()) / scale
                                        + 2 * Theme::current().largeSpacing);
    }
    return {width, kHeight};
}

void TextField::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const double scale = painter.scale();
    const RectF bounds = localRect();
    const bool focused = hasFocus();
    const int border = std::max(1, int(std::lround(scale)));

    painter.fillRoundedRect(bounds, theme.smallRadius, theme.alternateBackground);
    if (focused) {
        painter.strokeRoundedRect(bounds, theme.smallRadius, theme.highlight, border);
        // Concentric with the field.
        painter.strokeRoundedRect(bounds.adjusted(-kFocusRing, -kFocusRing, kFocusRing, kFocusRing),
                                  theme.smallRadius + kFocusRing, theme.highlight.withAlpha(0.25),
                                  std::max(1, int(std::lround(kFocusRing * scale))));
    }

    const double padding = theme.largeSpacing;
    const RectF content = bounds.adjusted(padding, 0, -padding, 0);
    const Rect device = painter.deviceRect(content);
    textlayout::Paragraph* text = d->layout(scale, isEnabled());
    const double width = device.width;

    // Keep the caret in sight, and no empty space past the end of the text.
    const double caretWidth = std::max(1.0, std::round(scale));
    const double caret = d->caretX(d->caretDisplayOffset());
    const double textWidth = text->getMaxIntrinsicWidth();
    if (caret - d->scrollX > width - caretWidth)
        d->scrollX = caret - width + caretWidth;
    if (caret - d->scrollX < 0)
        d->scrollX = caret;
    d->scrollX = std::max(0.0, std::min(d->scrollX, textWidth + caretWidth - width));

    const double originX = device.x - std::round(d->scrollX);
    const double originY = device.y + std::round((device.height - text->getHeight()) / 2);
    SkCanvas* canvas = painter.canvas();

    painter.save();
    painter.clip(content);
    if (focused && d->preedit.empty() && d->edit.hasSelection()) {
        SkPaint selection;
        selection.setColor(theme.highlight.argb());
        const size_t start = utf16Index(d->display, d->toDisplay(d->edit.selectionStart()));
        const size_t end = utf16Index(d->display, d->toDisplay(d->edit.selectionEnd()));
        for (const textlayout::TextBox& box : text->getRectsForRange(
                 unsigned(start), unsigned(end), textlayout::RectHeightStyle::kMax,
                 textlayout::RectWidthStyle::kTight)) {
            SkRect rect = box.rect.makeOffset(SkScalar(originX), SkScalar(originY));
            rect = SkRect::MakeLTRB(std::round(rect.left()), std::round(rect.top()),
                                    std::round(rect.right()), std::round(rect.bottom()));
            canvas->drawRect(rect, selection);
        }
    }
    if (d->display.empty() && !d->placeholder.empty()) {
        textlayout::Paragraph* placeholder = d->placeholderLayout(scale);
        placeholder->layout(SkScalar(width));
        placeholder->paint(canvas, SkScalar(device.x),
                           SkScalar(device.y + std::round((device.height - placeholder->getHeight()) / 2)));
    }
    text->paint(canvas, SkScalar(originX), SkScalar(originY));

    const Window* window = this->window();
    if (focused && window && window->hasKeyboardFocus() && d->caretVisible) {
        SkPaint paint;
        paint.setColor(theme.text.argb());
        const double x = std::round(originX + caret);
        canvas->drawRect(SkRect::MakeXYWH(SkScalar(x), SkScalar(originY), SkScalar(caretWidth),
                                          SkScalar(std::round(text->getHeight()))),
                         paint);
    }
    painter.restore();
}

void TextField::pointerPress(const PointerEvent& event)
{
    if (event.button == MouseButton::Right) {
        showContextMenu(event.position);
        return;
    }
    if (event.button != MouseButton::Left)
        return;
    const double scale = window() ? window()->scale() : 1.0;
    // Text inside the padding, as in paint, without the device-pixel snapping.
    const double x = (event.position.x - Theme::current().largeSpacing) * scale + d->scrollX;
    if (!d->preedit.empty()) {
        d->preedit.clear();
        d->dirty = true;
    }
    const size_t offset = d->fromDisplay(d->displayOffsetAt(x));
    d->selecting = true;
    d->granularity = std::clamp(event.clickCount, 1, 3);
    if (d->granularity == 3) {
        d->edit.selectAll();
    } else if (d->granularity == 2) {
        d->anchorWord = d->edit.wordAt(offset);
        d->edit.select(d->anchorWord.first, d->anchorWord.second);
    } else {
        d->edit.setCursor(offset, event.modifiers & ShiftModifier);
    }
    moved();
}

void TextField::pointerMove(const PointerEvent& event)
{
    if (!d->selecting || d->granularity == 3)
        return;
    const double scale = window() ? window()->scale() : 1.0;
    const double x = (event.position.x - Theme::current().largeSpacing) * scale + d->scrollX;
    const size_t offset = d->fromDisplay(d->displayOffsetAt(x));
    if (d->granularity == 2) {
        const auto word = d->edit.wordAt(offset);
        if (offset < d->anchorWord.first)
            d->edit.select(d->anchorWord.second, word.first);
        else
            d->edit.select(d->anchorWord.first, word.second);
    } else {
        d->edit.setCursor(offset, true);
    }
    moved();
}

void TextField::pointerRelease(const PointerEvent&)
{
    d->selecting = false;
}

void TextField::focusIn(FocusReason)
{
    moved();
}

void TextField::focusOut()
{
    if (d->blinkTimer) {
        Application::instance()->loop().stopTimer(d->blinkTimer);
        d->blinkTimer = 0;
    }
    d->edit.setCursor(d->edit.cursor());
    d->preedit.clear();
    d->dirty = true;
    requestPaint();
}

bool TextField::keyPress(const KeyEvent& event)
{
    const uint32_t modifiers = event.modifiers;
    const bool shift = modifiers & ShiftModifier;
    const bool control = modifiers & ControlModifier;
    const bool command = modifiers & (ControlModifier | AltModifier | SuperModifier);
    EditableText& edit = d->edit;

    const auto move = [this, &edit, shift](size_t position) {
        edit.setCursor(position, shift);
        moved();
        return true;
    };

    if (control && !(modifiers & (AltModifier | SuperModifier))) {
        switch (event.key) {
        case key::letter('a'):
            edit.selectAll();
            moved();
            return true;
        case key::letter('c'):
            copy();
            return true;
        case key::letter('x'):
            cut();
            return true;
        case key::letter('v'):
            paste();
            return true;
        case key::letter('z'):
            shift ? redo() : undo();
            return true;
        case key::letter('y'):
            redo();
            return true;
        default:
            break;
        }
    }

    switch (event.key) {
    case key::Left:
        if (!shift && edit.hasSelection() && !control)
            return move(edit.selectionStart());
        return move(control ? edit.previousWordStart(edit.cursor())
                            : edit.previousGrapheme(edit.cursor()));
    case key::Right:
        if (!shift && edit.hasSelection() && !control)
            return move(edit.selectionEnd());
        return move(control ? edit.nextWordEnd(edit.cursor()) : edit.nextGrapheme(edit.cursor()));
    case key::Home:
        return move(0);
    case key::End:
        return move(edit.text().size());
    case key::BackSpace:
        if (d->readOnly)
            return true;
        edit.deleteBackward(control);
        edited();
        return true;
    case key::Delete:
        if (d->readOnly)
            return true;
        edit.deleteForward(control);
        edited();
        return true;
    case key::Return:
    case key::KeypadEnter:
        if (onAccepted)
            onAccepted();
        return true;
    default:
        break;
    }

    if (!event.text.empty() && !command) {
        if (!d->readOnly) {
            edit.insert(event.text, true);
            edited();
        }
        return true;
    }
    return false;
}

bool TextField::acceptsTextInput() const
{
    return !d->readOnly && d->echo == EchoMode::Normal && isEnabled();
}

TextInputState TextField::textInputState() const
{
    TextInputState state;
    const std::string& text = d->edit.text();
    // A window of the text around the cursor, cut on character boundaries.
    size_t start = d->edit.selectionStart() > kSurroundingLimit / 2
            ? d->edit.selectionStart() - kSurroundingLimit / 2
            : 0;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80)
        --start;
    size_t end = std::min(text.size(), start + kSurroundingLimit);
    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
        --end;
    state.surroundingText = text.substr(start, end - start);
    state.cursor = d->edit.cursor() - start;
    state.anchor = d->edit.anchor() - start;

    const double scale = window() ? window()->scale() : 1.0;
    const double padding = Theme::current().largeSpacing;
    // The state follows an edit before it is painted; lay the new text out first.
    textlayout::Paragraph* paragraph = d->layout(scale, isEnabled());
    const double x = padding + (d->caretX(d->caretDisplayOffset()) - d->scrollX) / scale;
    const double height = paragraph->getHeight() / scale;
    const PointF topLeft = mapToWindow({x, (geometry().height - height) / 2});
    state.cursorRectangle = {topLeft.x, topLeft.y, 1, height};
    return state;
}

void TextField::textInput(const TextInputEvent& event)
{
    EditableText& edit = d->edit;
    d->preedit.clear();
    if (event.deleteBefore || event.deleteAfter) {
        const size_t cursor = edit.cursor();
        const size_t start = cursor > event.deleteBefore ? cursor - event.deleteBefore : 0;
        edit.erase(start, cursor + event.deleteAfter);
    }
    if (!event.commit.empty())
        edit.insert(event.commit, true);
    d->preedit = event.preedit;
    d->preeditCursor = event.preeditCursorBegin;
    edited();
}

void TextField::showContextMenu(PointF position)
{
    // FishUI's text menu, without the rows a read-only field has no use for.
    const bool editable = !d->readOnly;
    const bool clipboard = d->echo == EchoMode::Normal;
    const bool selection = d->edit.hasSelection();
    d->menu = std::make_unique<Menu>();
    Menu& menu = *d->menu;
    const auto addEditing = [&menu, editable](std::string text, std::function<void()> action,
                                              bool enabled) {
        const size_t index = menu.addItem(std::move(text), std::move(action));
        menu.setItemEnabled(index, enabled);
        menu.setItemVisible(index, editable);
    };
    addEditing("Undo", [this] { undo(); }, d->edit.canUndo());
    addEditing("Redo", [this] { redo(); }, d->edit.canRedo());
    if (editable)
        menu.addSeparator();
    addEditing("Cut", [this] { cut(); }, selection && clipboard);
    const size_t copyIndex = menu.addItem("Copy", [this] { copy(); });
    menu.setItemEnabled(copyIndex, selection && clipboard);
    addEditing("Paste", [this] { paste(); }, true);
    addEditing("Delete", [this] {
        d->edit.deleteBackward(false);
        edited();
    }, selection);
    menu.addSeparator();
    const size_t selectAllIndex = menu.addItem("Select All", [this] { selectAll(); });
    menu.setItemEnabled(selectAllIndex, !d->edit.text().empty());
    menu.popup(window(), mapToWindow(position));
}

void TextField::edited()
{
    moved();
    if (onTextChanged)
        onTextChanged();
}

void TextField::moved()
{
    d->dirty = true;
    d->caretVisible = true;
    auto& loop = Application::instance()->loop();
    if (d->blinkTimer)
        loop.stopTimer(d->blinkTimer);
    d->blinkTimer = 0;
    if (hasFocus()) {
        d->blinkTimer = loop.startTimer(kBlinkInterval, true, [this] {
            d->caretVisible = !d->caretVisible;
            requestPaint();
        });
    }
    requestPaint();
    updateTextInput();
}

} // namespace cutefish::ui
