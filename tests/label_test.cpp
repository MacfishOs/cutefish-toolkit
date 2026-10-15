// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "ui/label.h"
#include "ui/layout.h"
#include "ui/test_input.h"
#include "ui/window.h"

using namespace cutefish::ui;

namespace {

// A label alone in a headless window of the size, at the scale.
struct Hosted
{
    explicit Hosted(SizeF size, double scale = 1)
    {
        auto root = std::make_unique<Column>();
        label = root->addChild<Label>();
        label->setLayoutHints({.fillWidth = true});
        window.setContentView(std::move(root));
        window.resize(size);
        window.setScale(scale);
        window.show();
    }

    void layout() { window.grab(); }

    Window window;
    Label* label = nullptr;
};

} // namespace

TEST(LabelTest, ElidesTheMiddleOfALongLine)
{
    Hosted hosted({120, 40});
    hosted.label->setText("/media/user/A Very Long Volume Name/Projects");
    hosted.label->setElide(Label::Elide::Middle);
    hosted.layout();
    EXPECT_TRUE(hosted.label->isElided());
    const Rect ink = TestInput::inkRect(*hosted.label, 1);
    EXPECT_LE(ink.x + ink.width, 120);

    hosted.label->setText("/short");
    hosted.layout();
    EXPECT_FALSE(hosted.label->isElided());
}

TEST(LabelTest, MiddleAndRightElisionDiffer)
{
    Hosted middle({120, 40});
    Hosted right({120, 40});
    for (Hosted* hosted : {&middle, &right})
        hosted->label->setText("/media/user/A Very Long Volume Name/Projects");
    middle.label->setElide(Label::Elide::Middle);
    const Image a = middle.window.grab();
    const Image b = right.window.grab();
    EXPECT_TRUE(right.label->isElided());
    ASSERT_EQ(a.width(), b.width());
    ASSERT_EQ(a.height(), b.height());
    EXPECT_FALSE(std::equal(a.bits(), a.bits() + size_t(a.width()) * a.height(), b.bits()));
}

TEST(LabelTest, WrapsToAMaximumOfLines)
{
    Hosted hosted({80, 200});
    hosted.label->setText("one two three four five six seven eight nine ten eleven twelve");
    hosted.label->setWrap(true);
    const double unlimited = hosted.label->heightForWidth(80);
    hosted.label->setMaximumLineCount(2);
    const double limited = hosted.label->heightForWidth(80);
    hosted.label->setText("one");
    const double single = hosted.label->heightForWidth(80);
    EXPECT_GT(unlimited, limited);
    EXPECT_NEAR(limited, 2 * single, 1);
    hosted.label->setText("one two three four five six seven eight nine ten eleven twelve");
    hosted.layout();
    EXPECT_TRUE(hosted.label->isElided());
}

TEST(LabelTest, SpansCarryTheirOwnWeight)
{
    Hosted hosted({300, 40});
    hosted.label->setText("October 2026");
    const double regular = hosted.label->sizeHint().width;
    hosted.label->setSpans({{"October", 700, {}}, {" 2026", 0, {}}});
    EXPECT_EQ(hosted.label->text(), "October 2026");
    EXPECT_GT(hosted.label->sizeHint().width, regular);
}

TEST(LabelTest, PlacesItsBaselineAsAlignedLabelDoes)
{
    for (const double scale : {1.0, 1.25, 1.5, 2.0}) {
        Hosted hosted({200, 60}, scale);
        hosted.label->setText("Baseline");
        hosted.layout();
        // Qt's Text.baselineOffset: the ascent, unsnapped.
        EXPECT_DOUBLE_EQ(hosted.label->baselineOffset(), hosted.label->ascent());

        hosted.label->setBaseline(30);
        hosted.layout();
        // AlignedLabel's top margin is snapped; the baseline follows it.
        const double margin = std::round((30 - hosted.label->ascent()) * scale) / scale;
        EXPECT_NEAR(hosted.label->baselineOffset(), margin + hosted.label->ascent(), 1e-9) << scale;
        EXPECT_GT(hosted.label->sizeHint().height, 30);
        const double cap = hosted.label->capHeight();
        EXPECT_EQ(cap, std::round(cap));
        EXPECT_NEAR(hosted.label->capCenter(), 30 - cap / 2, 1 / scale);
    }
}

TEST(LabelTest, PaintsEveryWrappedLineInside)
{
    for (const double scale : {1.0, 1.5}) {
        for (const auto alignment : {Label::TextAlignment::Left, Label::TextAlignment::Center,
                                     Label::TextAlignment::Right}) {
            Window window;
            window.setBackgroundColor(Color::rgb(0xFFFFFF));
            auto root = std::make_unique<Column>();
            Label* label = root->addChild<Label>(
                "Shortcuts that use the Command key work in every application, and the key can be changed");
            label->setWrap(true);
            label->setTextAlignment(alignment);
            label->setLayoutHints({.preferredWidth = 160});
            window.setContentView(std::move(root));
            window.resize({200, 200});
            window.setScale(scale);
            window.show();
            const Image image = window.grab();
            const RectF box = label->geometry();
            EXPECT_GT(box.height, 50);
            // Ink in every line's band, and none outside the label.
            const double lineHeight = label->heightForWidth(160) / std::round(label->heightForWidth(160) / 17);
            for (double top = 0; top + lineHeight <= box.height + 0.5; top += lineHeight) {
                bool inked = false;
                for (int y = int(top * scale); y < int((top + lineHeight) * scale) && !inked; ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        if (image.pixel(x, y).r < 200) {
                            inked = true;
                            break;
                        }
                    }
                }
                EXPECT_TRUE(inked) << "line at " << top << " scale " << scale;
            }
            for (int y = 0; y < image.height(); ++y) {
                for (int x = int(std::ceil((box.x + box.width) * scale)) + 1; x < image.width(); ++x)
                    EXPECT_GE(image.pixel(x, y).r, 250) << x << "," << y << " scale " << scale;
            }
        }
    }
}

TEST(LabelTest, FitsItsOwnSizeHintInEveryMode)
{
    using Alignment = Label::TextAlignment;
    using Elide = Label::Elide;
    for (const double scale : {1.0, 1.25, 1.5, 2.0}) {
        for (const Alignment alignment : {Alignment::Left, Alignment::Center, Alignment::Right}) {
            for (const Elide elide : {Elide::None, Elide::Right, Elide::Middle}) {
                for (const int lines : {0, 2}) {
                    for (const char* text : {"Left", "Right", "Less warm", "2160 × 1440", "Built-in", "Today"}) {
                        Window window;
                        auto root = std::make_unique<Row>();
                        // A fractional x, which device rounding narrows.
                        root->setMargins({10.3, 0, 0, 0});
                        Label* label = root->addChild<Label>(text);
                        label->setTextAlignment(alignment);
                        label->setElide(elide);
                        label->setWrap(lines > 0);
                        label->setMaximumLineCount(lines);
                        window.setContentView(std::move(root));
                        window.resize({400, 100});
                        window.setScale(scale);
                        window.show();
                        window.grab();
                        const SizeF hint = label->sizeHint();
                        const std::string what = std::string(text) + " at " + std::to_string(scale);
                        EXPECT_LT(hint.width, 200) << what;
                        EXPECT_GT(hint.width, 10) << what;
                        EXPECT_EQ(label->geometry().width, hint.width) << what;
                        EXPECT_FALSE(label->isElided()) << what;
                        // Every glyph inside the label's device box.
                        const Rect ink = TestInput::inkRect(*label, scale);
                        EXPECT_GE(ink.x, 0) << what;
                        EXPECT_LE(ink.x + ink.width, int(std::ceil(hint.width * scale)) + 1) << what;
                    }
                }
            }
        }
    }
}
