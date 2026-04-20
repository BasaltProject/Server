#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginEvents.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::plugins;

namespace {
[[nodiscard]] const PluginEventType& breakType() {
    static const PluginEventType type{"block.break", true, {"player", "world", "block", "x", "y", "z"}, {"block"}};
    return type;
}

[[nodiscard]] const PluginEventType& chatType() {
    static const PluginEventType type{"player.chat", true, {"player", "message"}};
    return type;
}

[[nodiscard]] std::unique_ptr<PluginEventBus> makeBus() {
    auto bus = std::make_unique<PluginEventBus>();
    bus->declare(breakType());
    bus->declare(chatType());
    return bus;
}

void fill(PluginEvent& event, const std::vector<PluginEventValue>& values) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        event.set(static_cast<std::int32_t>(index), values[index]);
    }
}

[[nodiscard]] std::vector<PluginEventValue> stoneBreak(std::string world = "world",
                                                       std::string block = "minecraft:stone", std::int64_t y = 64) {
    return {std::string{"Whiron"}, std::move(world), std::move(block), std::int64_t{1}, y, std::int64_t{-3}};
}

[[nodiscard]] std::string joined(const std::vector<std::string>& values) {
    std::string text = "[";
    for (const std::string& value : values) {
        if (text.size() > 1) {
            text += ", ";
        }
        text += value;
    }
    return text + "]";
}
} // namespace

TEST_CASE("deciders run in priority order, then the observers") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> order;
    std::vector<PluginHandle> handles;
    const auto listen = [&](std::string name, PluginEventPriority priority, bool observer) {
        handles.push_back(bus->add(breakType(), "p", 0, priority, observer, false, {},
                                   [&order, name](PluginEvent&) { order.push_back(name); }));
    };
    listen("lateDecider", PluginEventPriority::Late, false);
    listen("observerFirst", PluginEventPriority::First, true);
    listen("firstDecider", PluginEventPriority::First, false);
    listen("normalDecider", PluginEventPriority::Normal, false);
    listen("observerLast", PluginEventPriority::Last, true);

    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    CHECK(bus->fire(event));
    CHECK(joined(order) == "[firstDecider, normalDecider, lateDecider, observerFirst, observerLast]");
    CHECK(bus->listening(breakType()));
}

TEST_CASE("two handlers at the same priority run in the order they registered") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> order;
    std::vector<PluginHandle> handles;
    static constexpr std::string_view kNames[] = {"h12", "h3",  "h10", "h5",  "h14", "h4", "h1",  "h9",  "h8",  "h17",
                                                  "h20", "h15", "h2",  "h13", "h11", "h6", "h19", "h16", "h18", "h7"};
    for (const std::string_view name : kNames) {
        handles.push_back(bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false, {},
                                   [&order, name = std::string{name}](PluginEvent&) { order.push_back(name); }));
    }
    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    bus->fire(event);
    CHECK(joined(order) == "[h12, h3, h10, h5, h14, h4, h1, h9, h8, h17, h20, h15, h2, h13, h11, h6, h19, h16, "
                           "h18, h7]");
}

TEST_CASE("a cancelled event stops there unless a handler asked for it anyway") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> order;
    std::vector<PluginHandle> handles;
    handles.push_back(bus->add(breakType(), "p", 0, PluginEventPriority::First, false, false, {},
                               [&order](PluginEvent&) { order.emplace_back("firstDecider"); }));
    handles.push_back(
        bus->add(breakType(), "p", 0, PluginEventPriority::Early, false, false, {}, [&order](PluginEvent& event) {
            order.emplace_back("canceller");
            event.cancel("not here");
        }));
    handles.push_back(bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false, {},
                               [&order](PluginEvent&) { order.emplace_back("normalDecider"); }));
    handles.push_back(bus->add(breakType(), "p", 0, PluginEventPriority::Last, false, true, {},
                               [&order](PluginEvent&) { order.emplace_back("wantsCancelled"); }));

    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    CHECK_FALSE(bus->fire(event));
    CHECK(joined(order) == "[firstDecider, canceller, wantsCancelled]");
    CHECK(event.cancelMessage() == "not here");
    CHECK(event.cancelled());

    // Firing again starts clean: the event is reused, so last time's answer must not linger.
    for (PluginHandle& handle : handles) {
        handle.remove();
    }
    fill(event, stoneBreak());
    CHECK(bus->fire(event));
    CHECK_FALSE(event.cancelled());
    CHECK_FALSE(event.cancelMessage().has_value());
}
