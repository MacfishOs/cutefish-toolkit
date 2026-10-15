// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "base/event_loop.h"
#include "ui/application.h"

cutefish::base::EventLoop* g_hostLoop = nullptr;

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    // As the compositor does: the host's loop exists first and is shared.
    cutefish::base::EventLoop hostLoop;
    g_hostLoop = &hostLoop;
    cutefish::ui::Application application(argc, argv,
                                          cutefish::ui::Application::Platform::Embedded);
    return RUN_ALL_TESTS();
}
