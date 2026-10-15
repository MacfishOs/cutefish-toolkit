// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <unistd.h>

#include "base/file.h"
#include "base/mime_apps.h"
#include "base/mime_database.h"
#include "base/paths.h"

using namespace cutefish::base;
namespace fs = std::filesystem;

namespace {

class MimeAppsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_directory = fs::temp_directory_path() / ("cutefish-mimeapps-" + std::to_string(getpid()));
        fs::create_directories(m_directory);
    }
    void TearDown() override { fs::remove_all(m_directory); }

    std::string path(const std::string& name) const { return (m_directory / name).string(); }

    fs::path m_directory;
};

} // namespace

TEST(MimeDatabaseTest, ResolvesAliasesAndParents)
{
    // shared-mime-info has these on every system the desktop runs on.
    EXPECT_EQ(mime::canonicalName("application/x-pdf"), "application/pdf");
    EXPECT_EQ(mime::canonicalName("application/pdf"), "application/pdf");
    EXPECT_EQ(mime::canonicalName("no/such-type"), "");
    const std::vector<std::string> aliases = mime::aliases("application/pdf");
    EXPECT_NE(std::find(aliases.begin(), aliases.end(), "application/x-pdf"), aliases.end());
    EXPECT_EQ(mime::parents("text/x-csrc"), std::vector<std::string>{"text/plain"});
    // Without parents of its own: application/octet-stream, as Qt falls back.
    EXPECT_EQ(mime::parents("image/png"), std::vector<std::string>{"application/octet-stream"});
    EXPECT_TRUE(mime::parents("inode/directory").empty());
    EXPECT_TRUE(mime::parents("application/octet-stream").empty());
}

TEST_F(MimeAppsTest, ListsLiveInTheConfigHome)
{
    EXPECT_EQ(mimeapps::userListPath(), paths::configHome() + "/mimeapps.list");
    const std::vector<std::string> lists = mimeapps::listPaths();
    ASSERT_FALSE(lists.empty());
    EXPECT_TRUE(lists.front().starts_with(paths::configHome()));
    EXPECT_TRUE(lists.front().ends_with("-mimeapps.list"));
}

TEST_F(MimeAppsTest, SetsDefaultsAsQtDoes)
{
    const std::string list = path("mimeapps.list");
    ASSERT_TRUE(file::writeAtomically(list, "# kept\n[Default Applications]\ntext/plain=old.desktop;\n"
                                            "[Added Associations]\ntext/plain=old.desktop;new.desktop;\n"
                                            "[Removed Associations]\ntext/plain=new.desktop;other.desktop;\n"));
    ASSERT_TRUE(mimeapps::setDefault(list, "text/plain", "new.desktop"));
    // What MimeApps::setDefault in cutefish-framework writes, byte for byte: the
    // file's last line break makes an empty line, which the group keeps.
    EXPECT_EQ(file::read(list).value_or(""),
              "# kept\n[Default Applications]\ntext/plain=new.desktop;\n"
              "[Added Associations]\ntext/plain=new.desktop;old.desktop;\n"
              "[Removed Associations]\n\ntext/plain=other.desktop;\n");
    EXPECT_EQ(mimeapps::associations(list, "Added Associations", "text/plain"),
              (std::vector<std::string>{"new.desktop", "old.desktop"}));
    EXPECT_FALSE(mimeapps::setDefault(list, "text/plain", "bad;id"));
    EXPECT_FALSE(fs::exists(list + ".lock"));
}

TEST_F(MimeAppsTest, KeepsTheTerminalList)
{
    const std::string list = path("xdg-terminals.list");
    ASSERT_TRUE(file::writeAtomically(list, "# preferred\n-excluded.desktop\nfirst.desktop:new-window\nsecond.desktop\n"));
    EXPECT_EQ(mimeapps::defaultTerminal(list), "first.desktop");
    ASSERT_TRUE(mimeapps::setDefaultTerminal(list, "second.desktop"));
    EXPECT_EQ(file::read(list).value_or(""),
              "second.desktop\n# preferred\n-excluded.desktop\nfirst.desktop:new-window\n");
    EXPECT_EQ(mimeapps::defaultTerminal(list), "second.desktop");
}
