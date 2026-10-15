// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "ui/application.h"

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    cutefish::ui::Application application(argc, argv,
                                          cutefish::ui::Application::Platform::Embedded);
    return RUN_ALL_TESTS();
}
