#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/net/ProxyLogin.hpp>
#include <basalt/server/runtime/net/SessionOutbox.hpp>
#include <basalt/wire/auth/forwarding/LegacyForwarding.hpp>
#include <basalt/wire/auth/forwarding/VelocityForwarding.hpp>

#include <doctest.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using namespace basalt::server::runtime::net;
namespace forwarding = basalt::wire::auth::forwarding;

namespace {

[[nodiscard]] SessionOutbox::Frame frameOf(std::size_t size, std::uint8_t fill) {
    return SessionOutbox::Frame(size, fill);
}

[[nodiscard]] ProxyLogin loginWith(forwarding::ForwardingMode mode, std::string secret = {}) {
    forwarding::ProxyForwarding configured;
    configured.mode = mode;
    configured.secret = std::move(secret);
    return ProxyLogin{configured, basalt::wire::common::Logger{"test"}};
}

} // namespace

TEST_CASE("nothing is queued until the outbox is open") {
    SessionOutbox outbox;
    CHECK_FALSE(outbox.isOpen());
    CHECK(outbox.pendingBytes() == 0);
    CHECK_FALSE(outbox.hasOverflowed());

    // Before the outbound thread exists the caller writes to the socket itself, which is what the
    // false says: login and configuration go out that way.
    CHECK_FALSE(outbox.offer(frameOf(16, 1)));
    CHECK(outbox.pendingBytes() == 0);
    CHECK(outbox.drain().empty());

    outbox.open();
    CHECK(outbox.isOpen());
    CHECK(outbox.offer(frameOf(16, 1)));
    CHECK(outbox.pendingBytes() == 16);
}

TEST_CASE("frames come out in the order they went in") {
    SessionOutbox outbox;
    outbox.open();
    for (std::uint8_t index = 0; index < 5; ++index) {
        CHECK(outbox.offer(frameOf(index + 1U, index)));
    }
    CHECK(outbox.pendingBytes() == 1 + 2 + 3 + 4 + 5);

    const std::deque<SessionOutbox::Frame> taken = outbox.drain();
    REQUIRE(taken.size() == 5);
    for (std::uint8_t index = 0; index < 5; ++index) {
        CAPTURE(index);
        CHECK(taken[index].size() == index + 1U);
        CHECK(taken[index][0] == index);
    }
    // Draining takes everything, so what is waiting afterwards is nothing at all.
    CHECK(outbox.pendingBytes() == 0);
    CHECK(outbox.drain().empty());
    CHECK(outbox.isOpen());
}

TEST_CASE("a client that stops draining overflows rather than being lied to") {
    SessionOutbox outbox{64};
    outbox.open();
    CHECK(outbox.offer(frameOf(40, 1)));
    CHECK_FALSE(outbox.hasOverflowed());
    CHECK(outbox.offer(frameOf(24, 2)));
    CHECK_FALSE(outbox.hasOverflowed());
    CHECK(outbox.pendingBytes() == 64);

    // One byte over: dropping frames would desynchronize the client anyway, so the outbox says so
    // and the connection is closed instead.
    CHECK(outbox.offer(frameOf(1, 3)));
    CHECK(outbox.hasOverflowed());
    CHECK(outbox.pendingBytes() == 64);
    CHECK(outbox.drain().size() == 2);

    // The flag stays: a connection that overflowed is going, whatever it does next.
    CHECK(outbox.hasOverflowed());

    CHECK(SessionOutbox::kDefaultLimitBytes == 8LL * 1024 * 1024);
}

TEST_CASE("a closed outbox forgets what was waiting and takes nothing more") {
    SessionOutbox outbox;
    outbox.open();
    outbox.offer(frameOf(32, 1));
    outbox.close();

    CHECK_FALSE(outbox.isOpen());
    CHECK(outbox.pendingBytes() == 0);
    CHECK(outbox.drain().empty());
    CHECK_FALSE(outbox.offer(frameOf(8, 2)));
}

TEST_CASE("frames from several threads still come out in one order") {
    SessionOutbox outbox;
    outbox.open();
    std::atomic<std::int32_t> ready{0};
    std::vector<std::thread> writers;
    writers.reserve(4);
    for (std::uint8_t writer = 0; writer < 4; ++writer) {
        writers.emplace_back([&outbox, &ready, writer] {
            ready.fetch_add(1);
            while (ready.load() < 4) {
            }
            for (std::int32_t index = 0; index < 250; ++index) {
                outbox.offer(frameOf(4, writer));
            }
        });
    }
    for (std::thread& writer : writers) {
        writer.join();
    }

    // Every frame is there once, and none of them is half of another: whatever the interleaving,
    // the queue is the order the socket sees.
    const std::deque<SessionOutbox::Frame> taken = outbox.drain();
    CHECK(taken.size() == 1000);
    std::vector<std::int32_t> perWriter(4, 0);
    for (const SessionOutbox::Frame& frame : taken) {
        REQUIRE(frame.size() == 4);
        CHECK(frame[0] == frame[3]);
        ++perWriter[frame[0]];
    }
    CHECK(perWriter == std::vector<std::int32_t>{250, 250, 250, 250});
    CHECK(outbox.pendingBytes() == 0);
}

TEST_CASE("which forwarding mode reads what") {
    const ProxyLogin none = loginWith(forwarding::ForwardingMode::None);
    CHECK_FALSE(none.enabled());
    CHECK_FALSE(none.readsHandshake());
    CHECK_FALSE(none.asksTheProxy());
    CHECK_FALSE(none.login().has_value());
    CHECK(none.transactionId() == ProxyLogin::kNoQuery);

    // The BungeeCord family reads the handshake host; Velocity asks over a login query.
    for (const forwarding::ForwardingMode mode :
         {forwarding::ForwardingMode::BungeeCord, forwarding::ForwardingMode::BungeeGuard,
          forwarding::ForwardingMode::SafeNet}) {
        const ProxyLogin legacy = loginWith(mode);
        CHECK(legacy.enabled());
        CHECK(legacy.readsHandshake());
        CHECK_FALSE(legacy.asksTheProxy());
    }

    const ProxyLogin velocity = loginWith(forwarding::ForwardingMode::Velocity, "a secret");
    CHECK(velocity.enabled());
    CHECK_FALSE(velocity.readsHandshake());
    CHECK(velocity.asksTheProxy());
    CHECK(velocity.channel() == "velocity:player_info");
    // parity: one byte naming the newest forwarding this server can read.
    CHECK(velocity.query() == std::vector<std::uint8_t>{4});
}

TEST_CASE("a query is only answered by the query that was asked") {
    ProxyLogin login = loginWith(forwarding::ForwardingMode::Velocity, "a secret");
    const std::int32_t asked = login.beginQuery();
    CHECK(login.transactionId() == asked);

    // An answer that names another query is not this query's answer, and the connection stays on
    // the one it is waiting for.
    const forwarding::ForwardingResult wrong = login.readAnswer(asked + 1, std::vector<std::uint8_t>{1, 2, 3});
    CHECK_FALSE(forwarding::accepted(wrong));
    CHECK(login.transactionId() == asked);

    // A client that does not know the channel answers with nothing: somebody came to this port
    // directly instead of through the proxy.
    const forwarding::ForwardingResult silent = login.readAnswer(asked, std::nullopt);
    CHECK_FALSE(forwarding::accepted(silent));
    CHECK(std::get<std::string>(silent) == "the connection did not answer the forwarding query");
    // And the query is over either way, so a second answer to it is not entertained.
    CHECK(login.transactionId() == ProxyLogin::kNoQuery);
    CHECK_FALSE(forwarding::accepted(login.readAnswer(asked, std::vector<std::uint8_t>{1})));

    // An answer that is not signed with the shared secret is refused, not parsed.
    ProxyLogin second = loginWith(forwarding::ForwardingMode::Velocity, "a secret");
    const std::int32_t again = second.beginQuery();
    CHECK_FALSE(forwarding::accepted(second.readAnswer(again, std::vector<std::uint8_t>(64, 0))));
}

TEST_CASE("a proxy that signs with the shared secret is who it says it is") {
    const std::vector<std::uint8_t> signed_ =
        forwarding::velocity::sign("a secret", "203.0.113.7", basalt::wire::common::Uuid{1, 2}, "Steve", {});

    ProxyLogin login = loginWith(forwarding::ForwardingMode::Velocity, "a secret");
    const std::int32_t asked = login.beginQuery();
    const forwarding::ForwardingResult answer = login.readAnswer(asked, signed_);
    REQUIRE(forwarding::accepted(answer));
    const forwarding::ForwardedLogin& who = std::get<forwarding::ForwardedLogin>(answer);
    CHECK(who.address == "203.0.113.7");
    CHECK(who.name == "Steve");
    CHECK(who.profileId == basalt::wire::common::Uuid{1, 2});

    // The same answer, signed and all, is refused once the query it belongs to is over: "not
    // waiting on one" has to be a number no query can be, or an answer to nothing is an answer.
    CHECK(login.transactionId() == ProxyLogin::kNoQuery);
    CHECK_FALSE(forwarding::accepted(login.readAnswer(ProxyLogin::kNoQuery, signed_)));
    CHECK_FALSE(forwarding::accepted(login.readAnswer(0, signed_)));

    // Signed with somebody else's secret, which is the whole point of signing it.
    ProxyLogin other = loginWith(forwarding::ForwardingMode::Velocity, "another secret");
    CHECK_FALSE(forwarding::accepted(other.readAnswer(other.beginQuery(), signed_)));
}

TEST_CASE("what the player is told, and what only the log is told") {
    ProxyLogin login = loginWith(forwarding::ForwardingMode::BungeeCord);

    const forwarding::ForwardingResult refused{std::string{"the host carried no forwarded data"}};
    const std::optional<std::string> message = login.accept(refused, "Steve", "203.0.113.7");
    REQUIRE(message.has_value());
    // The screen says the port is not for direct connections and nothing about which check failed:
    // saying that is telling whoever is guessing how close they got.
    CHECK(*message == ProxyLogin::kRefusedMessage);
    CHECK(message->find("forwarded data") == std::string::npos);
    CHECK_FALSE(login.login().has_value());

    // A rejection with nothing known about the connection still has something to log.
    CHECK(login.accept(refused, std::nullopt, std::nullopt).has_value());

    // And an accepted login is kept rather than merely allowed.
    const forwarding::ForwardedLogin who{.host = "play.example.com",
                                         .address = "203.0.113.7",
                                         .profileId = basalt::wire::common::Uuid{1, 2},
                                         .name = "Steve",
                                         .properties = {}};
    CHECK_FALSE(login.accept(forwarding::ForwardingResult{who}, "Steve", "203.0.113.7").has_value());
    REQUIRE(login.login().has_value());
    CHECK(login.login()->address == "203.0.113.7");
    CHECK(login.login()->name == "Steve");
}

TEST_CASE("the handshake host is read through the configured mode") {
    forwarding::ProxyForwarding configured;
    configured.mode = forwarding::ForwardingMode::BungeeCord;
    ProxyLogin login{configured, basalt::wire::common::Logger{"test"}};

    const std::string host =
        forwarding::legacy::encode("play.example.com", "203.0.113.7", basalt::wire::common::Uuid{1, 2}, {});
    const forwarding::ForwardingResult read = login.readHandshake(host);
    REQUIRE(forwarding::accepted(read));
    CHECK(std::get<forwarding::ForwardedLogin>(read).host == "play.example.com");
    CHECK(std::get<forwarding::ForwardedLogin>(read).address == "203.0.113.7");

    // A host with nothing appended is somebody connecting straight to the port.
    CHECK_FALSE(forwarding::accepted(login.readHandshake("play.example.com")));
}
