// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "test_views.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/window.h"

using namespace cutefish::ui;
using test::Box;

// Lays the content out at the given size by rendering it once.
template<typename T>
static T* host(Window& window, std::unique_ptr<T> content, SizeF size)
{
    T* view = content.get();
    window.resize(size);
    window.setContentView(std::move(content));
    window.show();
    window.grab();
    return view;
}

TEST(LayoutTest, ColumnStacksPreferredSizes)
{
    Window window;
    auto content = std::make_unique<Column>();
    content->setSpacing(5);
    content->setMargins(Margins::all(10));
    Box* first = content->addChild<Box>(SizeF{50, 10});
    Box* second = content->addChild<Box>(SizeF{60, 20});
    Box* third = content->addChild<Box>(SizeF{70, 30});
    Column* column = host(window, std::move(content), {200, 200});

    EXPECT_EQ(first->geometry(), (RectF{10, 10, 50, 10}));
    EXPECT_EQ(second->geometry(), (RectF{10, 25, 60, 20}));
    EXPECT_EQ(third->geometry(), (RectF{10, 50, 70, 30}));
    EXPECT_EQ(column->sizeHint(), (SizeF{90, 90}));
}

TEST(LayoutTest, ColumnSharesLeftoverByStretch)
{
    Window window;
    auto content = std::make_unique<Column>();
    Box* top = content->addChild<Box>(SizeF{10, 20});
    Box* one = content->addChild<Box>(SizeF{10, 0});
    one->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1});
    Box* two = content->addChild<Box>(SizeF{10, 0});
    two->setLayoutHints({.fillHeight = true, .stretch = 2});
    host(window, std::move(content), {100, 110});

    EXPECT_EQ(top->geometry(), (RectF{0, 0, 10, 20}));
    EXPECT_EQ(one->geometry(), (RectF{0, 20, 100, 30}));
    EXPECT_EQ(two->geometry(), (RectF{0, 50, 10, 60}));
}

TEST(LayoutTest, RowCentresAcrossByDefault)
{
    Window window;
    auto content = std::make_unique<Row>();
    content->setSpacing(4);
    Box* small = content->addChild<Box>(SizeF{20, 10});
    Box* tall = content->addChild<Box>(SizeF{30, 30});
    View* stretch = content->addStretch();
    Box* end = content->addChild<Box>(SizeF{10, 10});
    end->setLayoutHints({.alignment = Alignment::End});
    host(window, std::move(content), {100, 50});

    EXPECT_EQ(small->geometry(), (RectF{0, 20, 20, 10}));
    EXPECT_EQ(tall->geometry(), (RectF{24, 10, 30, 30}));
    EXPECT_EQ(stretch->geometry().width, 100 - 20 - 30 - 10 - 3 * 4);
    EXPECT_EQ(end->geometry(), (RectF{90, 40, 10, 10}));
}

TEST(LayoutTest, HiddenChildrenTakeNoSpace)
{
    Window window;
    auto content = std::make_unique<Column>();
    content->setSpacing(10);
    content->addChild<Box>(SizeF{10, 10});
    Box* hidden = content->addChild<Box>(SizeF{10, 10});
    hidden->setVisible(false);
    Box* last = content->addChild<Box>(SizeF{10, 10});
    host(window, std::move(content), {100, 100});

    EXPECT_EQ(last->geometry().y, 20);
}

TEST(LayoutTest, GridSizesTracksByLargestChild)
{
    Window window;
    auto content = std::make_unique<Grid>(2);
    content->setColumnSpacing(10);
    content->setRowSpacing(5);
    Box* a = content->addChild<Box>(SizeF{40, 10});
    Box* b = content->addChild<Box>(SizeF{20, 20});
    b->setLayoutHints({.fillWidth = true});
    Box* c = content->addChild<Box>(SizeF{60, 30});
    Box* d = content->addChild<Box>(SizeF{10, 10});
    host(window, std::move(content), {200, 100});

    EXPECT_EQ(a->geometry(), (RectF{0, 5, 40, 10}));
    EXPECT_EQ(b->geometry(), (RectF{70, 0, 130, 20}));
    EXPECT_EQ(c->geometry(), (RectF{0, 25, 60, 30}));
    EXPECT_EQ(d->geometry(), (RectF{70, 35, 10, 10}));
}

TEST(LayoutTest, WrappedLabelGrowsWhenNarrow)
{
    Window window;
    auto content = std::make_unique<Column>();
    Label* label = content->addChild<Label>(
        "Several words that will not fit on one line once the column gets narrow");
    label->setWrap(true);
    label->setLayoutHints({.fillWidth = true});
    host(window, std::move(content), {600, 200});

    const double wide = label->heightForWidth(600);
    const double narrow = label->heightForWidth(120);
    EXPECT_GT(narrow, wide * 2);
    EXPECT_EQ(label->geometry().height, wide);
}

TEST(LayoutTest, DumpListsLogicalAndDeviceGeometry)
{
    Window window;
    window.setScale(1.5);
    auto content = std::make_unique<Column>();
    content->setName("content");
    content->setMargins(Margins::all(5));
    content->addChild<Box>(SizeF{11, 11})->setName("box");
    host(window, std::move(content), {21, 21});

    EXPECT_EQ(window.layoutDump(),
              "Column \"content\" logical 0 0 21 21 device 0 0 32 32\n"
              "  test::Box \"box\" logical 5 5 11 11 device 8 8 16 16\n");
}
