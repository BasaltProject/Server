#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/ServerWatchdog.hpp>
#include <basalt/server/runtime/net/ProxyProtocol.hpp>

#include <doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

using namespace basalt::server::runtime;

namespace {

[[nodiscard]] std::function<std::size_t(std::span<std::uint8_t>)> feed(std::vector<std::uint8_t> bytes) {
    return [bytes = std::move(bytes), at = std::size_t{0}](std::span<std::uint8_t> into) mutable {
        const std::size_t count = std::min(into.size(), bytes.size() - at);
        std::memcpy(into.data(), bytes.data() + at, count);
        at += count;
        return count;
    };
}

} // namespace

TEST_CASE("a PROXY header of either version yields the client address") {
    constexpr std::string_view kV1 = "PROXY TCP4 192.168.1.5 10.0.0.1 56324 25565\r\n";
    const auto v1 = net::proxyProtocol::readHeader(feed({kV1.begin(), kV1.end()}));
    REQUIRE(v1.has_value());
    REQUIRE(v1->has_value());
    CHECK((*v1)->address == "192.168.1.5");
    CHECK((*v1)->port == 56324);

    std::vector<std::uint8_t> v2{0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A, 0x21, 0x11,
                                 0x00, 0x0C, 192,  168,  1,    5,    10,   0,    0,    1,    0xDC, 0x04, 0x63, 0xDD};
    const auto binary = net::proxyProtocol::readHeader(feed(v2));
    REQUIRE(binary.has_value());
    REQUIRE(binary->has_value());
    CHECK((*binary)->address == "192.168.1.5");
    CHECK((*binary)->port == 56324);

    CHECK_FALSE(
        net::proxyProtocol::readHeader(feed({0x16, 0x03, 0x01, 0x02, 0x00, 0x01, 0x00, 0x01, 0xFC, 0x03, 0x03, 0x00}))
            .has_value());
}

TEST_CASE("the watchdog gives up on a stalled tick and leaves a ticking server alone") {
    using namespace std::chrono_literals;
    std::atomic<std::int64_t> ticks{0};
    std::atomic<int> fired{0};
    {
        ServerWatchdog dog{200ms, [&ticks] { return ticks.load(); }, [&fired](double) { fired++; }, 10ms};
        for (int i = 0; i < 40; ++i) {
            ticks++;
            std::this_thread::sleep_for(20ms);
        }
        CHECK(fired.load() == 0);
        std::this_thread::sleep_for(400ms);
        CHECK(fired.load() == 1);
    }
}
