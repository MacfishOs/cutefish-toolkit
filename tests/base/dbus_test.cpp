// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>
#include <unistd.h>

#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/timer.h"

using namespace cutefish::base;
using namespace cutefish::base::dbus;
using namespace std::chrono_literals;

static constexpr const char* kService = "org.cutefish.ToolkitTest";
static constexpr const char* kPath = "/Test";
static constexpr const char* kInterface = "org.cutefish.ToolkitTest";

TEST(DBusTypesTest, Signatures)
{
    EXPECT_EQ(Type<VariantMap>::signature(), "a{sv}");
    EXPECT_EQ((Type<std::map<ObjectPath, std::map<std::string, VariantMap>>>::signature()), "a{oa{sa{sv}}}");
    EXPECT_EQ((Type<std::tuple<std::string, int32_t, std::vector<uint8_t>>>::signature()), "(siay)");
    EXPECT_EQ((signatureOf<bool, uint32_t, const char*, ObjectPath>()), "buso");
}

TEST(DBusTypesTest, VariantsConvertBack)
{
    const VariantMap map{{"Name", std::string("Office")}, {"Strength", uint8_t(72)},
                         {"Ssid", std::vector<uint8_t>{'a', 'b'}}, {"Nested", Type<Variant>::toVariant(int32_t(5))}};
    const Variant variant(map);
    EXPECT_EQ(variant.signature(), "a{sv}");
    const auto back = variant.get<VariantMap>();
    ASSERT_TRUE(back);
    EXPECT_EQ(back->at("Name").get<std::string>(), "Office");
    EXPECT_EQ(back->at("Strength").get<uint8_t>(), 72);
    EXPECT_EQ(back->at("Ssid").bytes(), "ab");
    // Type mismatches give nothing rather than a converted value.
    EXPECT_FALSE(back->at("Strength").get<uint32_t>());
    EXPECT_EQ(back->at("Nested").get<Variant>()->get<int32_t>(), 5);
    EXPECT_EQ(*back, map);
}

namespace {

// A service on a thread of its own, so the test thread can block on calls.
class TestService
{
public:
    TestService()
    {
        std::promise<bool> ready;
        m_thread = std::thread([this, &ready] { run(ready); });
        m_ready = ready.get_future().get();
    }
    ~TestService()
    {
        m_handle.post([this] { m_loop->quit(); });
        m_thread.join();
    }

    bool isReady() const { return m_ready; }
    void releaseName()
    {
        std::promise<void> done;
        m_handle.post([this, &done] {
            m_connection->releaseName(kService);
            done.set_value();
        });
        done.get_future().get();
    }

private:
    void run(std::promise<bool>& ready)
    {
        EventLoop loop;
        m_loop = &loop;
        m_handle = loop.handle();
        std::unique_ptr<Connection> connection = Connection::open(Connection::Bus::Session, &loop);
        if (!connection || !connection->requestName(kService)) {
            ready.set_value(false);
            return;
        }
        m_connection = connection.get();

        Object object(connection.get(), kPath, kInterface);
        object.addMethod("Echo", [](const std::string& text, int32_t number) {
            return std::make_tuple(text + "!", number * 2);
        });
        object.addMethod("Sum", [](const std::vector<int32_t>& numbers) {
            int64_t sum = 0;
            for (const int32_t number : numbers)
                sum += number;
            return sum;
        });
        object.addMethod("EchoMap", [](const VariantMap& map) { return map; });
        object.addMethod("Fail", "", "", [](MethodCall& call) {
            call.replyError("org.cutefish.ToolkitTest.Error.Failed", "As asked");
        });
        object.addMethod("Later", "u", "u", [&loop](MethodCall& call) {
            uint32_t value = 0;
            call.read(value);
            loop.startTimer(20ms, false, [call, value]() mutable { call.reply(value + 1); });
        });
        object.addMethod("ReadFd", [](const UnixFd& fd) {
            char buffer[16] = {};
            const ssize_t size = ::read(fd.fd(), buffer, sizeof(buffer) - 1);
            return std::string(buffer, size > 0 ? size_t(size) : 0);
        });
        object.addMethod("Emit", [&object](const std::string& text) { object.emitSignal("Changed", text); });
        object.addMethod("Interactive", "", "b", [](MethodCall& call) {
            call.reply(call.message().allowsInteractiveAuthorization());
        });
        object.addProperty("Name", [] { return std::string("Test"); });
        object.addProperty("Count", [this] { return m_count; }, [this, &object](int32_t count) {
            if (count < 0)
                return false;
            m_count = count;
            object.emitPropertiesChanged({"Count"});
            return true;
        });
        object.addSignal("Changed", "s");
        object.addMethod(
                "Named",
                [](const std::string& text, int32_t count) { return std::tuple<std::string, bool>(text, count > 0); },
                ArgumentNames{{"text", "count"}, {"result", "ok"}});
        object.addMethod("NamedRaw", "a{sv}(ii)", "", [](MethodCall& call) { call.reply(); },
                         ArgumentNames{{"options", "pair"}, {}});
        object.addMethod("Mismatched", "ss", "", [](MethodCall& call) { call.reply(); },
                         ArgumentNames{{"only"}, {}});
        object.addSignal("NamedChanged", "sa(ii)", {"name", "items"});
        ready.set_value(object.publish());
        loop.run();
        m_connection = nullptr;
    }

    std::thread m_thread;
    EventLoop* m_loop = nullptr;
    LoopHandle m_handle;
    Connection* m_connection = nullptr;
    int32_t m_count = 3;
    bool m_ready = false;
};

class DBusTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_connection = Connection::open(Connection::Bus::Session, &m_loop);
        if (!m_connection || m_connection->uniqueName().empty())
            GTEST_SKIP() << "No session bus; run the tests in dbus-run-session";
        m_service = std::make_unique<TestService>();
        ASSERT_TRUE(m_service->isReady());
        m_proxy = Proxy(m_connection.get(), kService, kPath, kInterface);
    }

    // Runs the loop until quit or the time is up.
    void runFor(std::chrono::milliseconds limit)
    {
        const int timer = m_loop.startTimer(limit, false, [this] { m_loop.quit(); });
        m_loop.run();
        m_loop.stopTimer(timer);
    }

    EventLoop m_loop;
    std::unique_ptr<Connection> m_connection;
    std::unique_ptr<TestService> m_service;
    Proxy m_proxy;
};

} // namespace

TEST_F(DBusTest, CallsWithTypedArguments)
{
    const Reply echo = m_proxy.call("Echo", "hi", int32_t(21));
    ASSERT_FALSE(echo.isError()) << echo.error().message;
    std::string text;
    int32_t number = 0;
    ASSERT_TRUE(echo.read(text, number));
    EXPECT_EQ(text, "hi!");
    EXPECT_EQ(number, 42);

    const VariantMap map{{"Id", std::string("Home")}, {"Ports", std::vector<uint16_t>{80, 443}}};
    EXPECT_EQ(m_proxy.call("EchoMap", map).value<VariantMap>(), map);

    const Reply wrong = m_proxy.call("Echo", int32_t(1));
    EXPECT_TRUE(wrong.isError());
}

TEST_F(DBusTest, AsyncCallsAndErrors)
{
    int64_t sum = 0;
    Error error;
    uint32_t later = 0;
    int replies = 0;
    auto done = [&] {
        if (++replies == 3)
            m_loop.quit();
    };
    PendingCall first = m_proxy.callAsync("Sum", [&](const Reply& reply) {
        sum = reply.value<int64_t>().value_or(-1);
        done();
    }, std::vector<int32_t>{1, 2, 3});
    PendingCall second = m_proxy.callAsync("Fail", [&](const Reply& reply) {
        error = reply.error();
        done();
    });
    PendingCall third = m_proxy.callAsync("Later", [&](const Reply& reply) {
        later = reply.value<uint32_t>().value_or(0);
        done();
    }, uint32_t(9));
    runFor(2s);
    EXPECT_EQ(sum, 6);
    EXPECT_EQ(error.name, "org.cutefish.ToolkitTest.Error.Failed");
    EXPECT_EQ(error.message, "As asked");
    EXPECT_EQ(later, 10u);
}

TEST_F(DBusTest, DroppedPendingCallNeverCallsBack)
{
    bool called = false;
    {
        PendingCall call = m_proxy.callAsync("Later", [&](const Reply&) { called = true; }, uint32_t(1));
    }
    runFor(100ms);
    EXPECT_FALSE(called);
}

TEST_F(DBusTest, PropertiesAndTheirChanges)
{
    EXPECT_EQ(m_proxy.property<std::string>("Name"), "Test");
    EXPECT_EQ(m_proxy.property<int32_t>("Count"), 3);
    const auto all = m_proxy.getAll().value<VariantMap>();
    ASSERT_TRUE(all);
    EXPECT_EQ(all->size(), 2u);

    VariantMap changed;
    Match match = m_proxy.onPropertiesChanged([&](const VariantMap& properties, const std::vector<std::string>&) {
        changed = properties;
        m_loop.quit();
    });
    // The match is added asynchronously; a round trip makes sure it is in.
    EXPECT_TRUE(m_proxy.call("Echo", "", int32_t(0)));
    EXPECT_FALSE(m_proxy.setProperty("Count", int32_t(7)).isError());
    EXPECT_TRUE(m_proxy.setProperty("Count", int32_t(-1)).isError());
    EXPECT_TRUE(m_proxy.setProperty("Name", std::string("x")).isError());
    runFor(2s);
    ASSERT_EQ(changed.count("Count"), 1u);
    EXPECT_EQ(changed.at("Count").get<int32_t>(), 7);
}

TEST_F(DBusTest, SignalsReachSubscribers)
{
    std::string received;
    Match match = m_proxy.onSignal("Changed", [&](const Message& signal) {
        received = signal.argument<std::string>().value_or("");
        m_loop.quit();
    });
    EXPECT_TRUE(m_proxy.call("Echo", "", int32_t(0)));
    EXPECT_TRUE(m_proxy.call("Emit", "news"));
    runFor(2s);
    EXPECT_EQ(received, "news");

    received.clear();
    match.reset();
    EXPECT_TRUE(m_proxy.call("Emit", "again"));
    runFor(100ms);
    EXPECT_TRUE(received.empty());
}

TEST_F(DBusTest, InteractiveAuthorizationReachesTheService)
{
    EXPECT_EQ(m_proxy.call("Interactive").value<bool>(), false);
    Proxy interactive = m_proxy;
    interactive.setInteractiveAuthorizationAllowed(true);
    interactive.setTimeout(std::chrono::seconds(120));
    EXPECT_EQ(interactive.call("Interactive").value<bool>(), true);
    bool asynchronous = false;
    PendingCall call = interactive.callAsync("Interactive", [&](const Reply& reply) {
        asynchronous = reply.value<bool>().value_or(false);
        m_loop.quit();
    });
    runFor(2s);
    EXPECT_TRUE(asynchronous);
}

TEST_F(DBusTest, FileDescriptorsPass)
{
    int pipe[2];
    ASSERT_EQ(::pipe(pipe), 0);
    ASSERT_EQ(::write(pipe[1], "piped", 5), 5);
    close(pipe[1]);
    const Reply reply = m_proxy.call("ReadFd", UnixFd::adopt(pipe[0]));
    EXPECT_EQ(reply.value<std::string>(), "piped");
}

TEST_F(DBusTest, NamesAreWatchedAndOwnedOnce)
{
    EXPECT_FALSE(m_connection->requestName(kService));

    std::vector<std::string> owners;
    NameWatch watch = m_connection->watchName(kService, [&](const std::string& owner) {
        owners.push_back(owner);
        m_loop.quit();
    });
    runFor(2s);
    ASSERT_EQ(owners.size(), 1u);
    EXPECT_FALSE(owners[0].empty());

    m_service->releaseName();
    runFor(2s);
    ASSERT_EQ(owners.size(), 2u);
    EXPECT_TRUE(owners[1].empty());

    // Free again, so this connection takes it.
    EXPECT_TRUE(m_connection->requestName(kService));
}

TEST_F(DBusTest, SharedConnectionLivesWithItsLoop)
{
    Connection* shared = nullptr;
    {
        EventLoop loop;
        shared = Connection::session(&loop);
        ASSERT_NE(shared, nullptr);
        EXPECT_EQ(Connection::session(&loop), shared);
        Proxy proxy(shared, kService, kPath, kInterface);
        EXPECT_EQ(proxy.call("Echo", "a", int32_t(1)).value<std::string>(), "a!");
    }
    EXPECT_NE(Connection::session(&m_loop), nullptr);
}

TEST_F(DBusTest, IntrospectionCarriesArgumentNames)
{
    Proxy introspectable(m_connection.get(), kService, kPath, "org.freedesktop.DBus.Introspectable");
    const std::optional<std::string> xml = introspectable.call("Introspect").value<std::string>();
    ASSERT_TRUE(xml);
    const auto has = [&xml](const std::string& text) { return xml->find(text) != std::string::npos; };
    EXPECT_TRUE(has(R"x(<arg type="s" name="text" direction="in"/>)x")) << *xml;
    EXPECT_TRUE(has(R"x(<arg type="i" name="count" direction="in"/>)x"));
    EXPECT_TRUE(has(R"x(<arg type="s" name="result" direction="out"/>)x"));
    EXPECT_TRUE(has(R"x(<arg type="b" name="ok" direction="out"/>)x"));
    EXPECT_TRUE(has(R"x(<arg type="a{sv}" name="options" direction="in"/>)x"));
    EXPECT_TRUE(has(R"x(<arg type="(ii)" name="pair" direction="in"/>)x"));
    EXPECT_TRUE(has(R"x(<arg type="s" name="name"/>)x"));
    EXPECT_TRUE(has(R"x(<arg type="a(ii)" name="items"/>)x"));
    // A list that does not match its signature is dropped.
    EXPECT_FALSE(has(R"x(name="only")x"));
    // Members without names still work.
    EXPECT_TRUE(has(R"x(<method name="Echo">)x"));

    const Reply named = m_proxy.call("Named", "x", int32_t(2));
    std::string text;
    bool ok = false;
    ASSERT_TRUE(named.read(text, ok));
    EXPECT_EQ(text, "x");
    EXPECT_TRUE(ok);
}
