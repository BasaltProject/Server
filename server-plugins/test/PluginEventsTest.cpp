#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginEvents.hpp>

#include <doctest.h>

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

[[nodiscard]] std::string asText(const PluginEventValue& value) {
    if (const std::string* text = std::get_if<std::string>(&value)) {
        return *text;
    }
    return "null";
}

} // namespace

TEST_CASE("what may be listened to, and what an event says about itself") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> names;
    for (const PluginEventType* type : bus->declared()) {
        names.push_back(type->name() + "/" + (type->cancellable() ? "true" : "false") + "/" + joined(type->fields()) +
                        "/" + (type->isMutable("block") ? "block" : ""));
    }
    CHECK(joined(names) == "[block.break/true/[player, world, block, x, y, z]/block, "
                           "player.chat/true/[player, message]/]");
    CHECK(bus->typeOf("block.break") != nullptr);
    CHECK(bus->typeOf("nope") == nullptr);

    CHECK(breakType().indexOf("y") == 4);
    CHECK(breakType().indexOf("nope") == -1);
    CHECK_FALSE(bus->listening(breakType()));

    // Nobody listening: the fast path answers yes and leaves nothing behind.
    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    CHECK(bus->fire(event));
    CHECK(&bus->event(breakType()) == &event);
}

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
    // Twenty, not three: a small list comes back in order from almost any sort, so a stable one only
    // proves itself past the size where the algorithm changes. The names are shuffled so that
    // registration order and alphabetical order are not the same answer.
    static constexpr std::string_view kNames[] = {"h12", "h3",  "h10", "h5",  "h14", "h4", "h1",  "h9",  "h8",  "h17",
                                                  "h20", "h15", "h2",  "h13", "h11", "h6", "h19", "h16", "h18", "h7"};
    for (const std::string_view name : kNames) {
        handles.push_back(bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false, {},
                                   [&order, name = std::string{name}](PluginEvent&) { order.push_back(name); }));
    }
    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    bus->fire(event);
    // Registration order, not whatever order the sort happened to leave them in.
    CHECK(joined(order) == "[h12, h3, h10, h5, h14, h4, h1, h9, h8, h17, h20, h15, h2, h13, h11, h6, h19, h16, "
                           "h18, h7]");
}

TEST_CASE("what a handler sees of the event while it is running") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> inside;
    const PluginHandle handle =
        bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false, {}, [&inside](PluginEvent& event) {
            inside.push_back(std::string{"live="} + (event.live() ? "true" : "false") +
                             " generation=" + std::to_string(event.generation()));
        });

    PluginEvent& event = bus->event(breakType());
    for (std::int32_t round = 0; round < 3; ++round) {
        fill(event, stoneBreak());
        bus->fire(event);
    }
    CHECK(joined(inside) == "[live=true generation=1, live=true generation=2, live=true generation=3]");
    CHECK_FALSE(event.live());
}

TEST_CASE("removing one handle leaves the others alone") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> left;
    PluginHandle goes = bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false, {},
                                 [&left](PluginEvent&) { left.emplace_back("goes"); });
    const PluginHandle stays = bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false, {},
                                        [&left](PluginEvent&) { left.emplace_back("stays"); });
    goes.remove();

    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    bus->fire(event);
    CHECK(joined(left) == "[stays]");
    CHECK(bus->listening(breakType()));
}

TEST_CASE("a handler may register others while the event is being dispatched") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> grown;
    std::vector<PluginHandle> handles;
    handles.push_back(bus->add(breakType(), "p", 0, PluginEventPriority::First, false, false, {}, [&](PluginEvent&) {
        grown.emplace_back("first");
        // Enough to make the list grow under the dispatch, which is what
        // a handler held by reference rather than copied would not
        // survive.
        for (std::int32_t extra = 1; extra <= 8; ++extra) {
            handles.push_back(
                bus->add(breakType(), "p", 0, PluginEventPriority::Last, false, false, {},
                         [&grown, extra](PluginEvent&) { grown.push_back("extra" + std::to_string(extra)); }));
        }
    }));

    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    bus->fire(event);
    CHECK(grown.size() == 9);
    CHECK(grown.front() == "first");
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

TEST_CASE("a filter keeps a handler asleep for the events it did not ask about") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> seen;
    const PluginHandle diamond = bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false,
                                          {{"block", std::string{"minecraft:diamond_ore"}}},
                                          [&seen](PluginEvent&) { seen.emplace_back("diamond"); });
    const PluginHandle netherTen = bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false,
                                            {{"world", std::string{"nether"}}, {"y", std::int64_t{10}}},
                                            [&seen](PluginEvent&) { seen.emplace_back("netherTen"); });

    PluginEvent& event = bus->event(breakType());
    for (const std::string block : {"minecraft:stone", "minecraft:diamond_ore"}) {
        fill(event, stoneBreak("world", block));
        bus->fire(event);
    }
    fill(event, stoneBreak("nether", "minecraft:stone", 10));
    bus->fire(event);
    CHECK(joined(seen) == "[diamond, netherTen]");
}

TEST_CASE("unloading a plugin takes its handlers and leaves the others") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    std::vector<std::string> ran;
    const PluginHandle a = bus->add(breakType(), "arena", 0, PluginEventPriority::Normal, false, false, {},
                                    [&ran](PluginEvent&) { ran.emplace_back("arena"); });
    const PluginHandle b = bus->add(breakType(), "shop", 0, PluginEventPriority::Normal, false, false, {},
                                    [&ran](PluginEvent&) { ran.emplace_back("shop"); });
    const PluginHandle c = bus->add(chatType(), "arena", 0, PluginEventPriority::Normal, false, false, {},
                                    [&ran](PluginEvent&) { ran.emplace_back("arenaChat"); });
    bus->removeAll("arena");

    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    bus->fire(event);
    bus->fire(bus->event(chatType()));
    CHECK(joined(ran) == "[shop]");
    CHECK(bus->listening(breakType()));
    CHECK_FALSE(bus->listening(chatType()));
}

TEST_CASE("what a handler writes is what the server reads back") {
    const std::unique_ptr<PluginEventBus> bus = makeBus();
    const PluginHandle handle =
        bus->add(breakType(), "p", 0, PluginEventPriority::Normal, false, false, {},
                 [](PluginEvent& event) { event.set("block", std::string{"minecraft:cobblestone"}); });

    PluginEvent& event = bus->event(breakType());
    fill(event, stoneBreak());
    bus->fire(event);
    CHECK(asText(event.get("block")) == "minecraft:cobblestone");
    CHECK(asText(event.get(2)) == "minecraft:cobblestone");
    CHECK(asText(event.get("nope")) == "null");
    // The frozen Kotlin reads its array unchecked and throws here, which is its own bug rather than
    // anything to be faithful to: a plugin asking for a field that is not there gets nil.
    CHECK(asText(event.get(99)) == "null");
    event.set(99, std::string{"ignored"});
    CHECK(asText(event.get(99)) == "null");
    // A field this event does not have resolves to -1, which the same guard has to catch.
    event.set("nope", std::string{"ignored"});
    CHECK(asText(event.get("block")) == "minecraft:cobblestone");
    CHECK_FALSE(event.live());
}

TEST_CASE("the five priorities, and the one Bukkit has that this does not") {
    CHECK(priorityName(PluginEventPriority::First) == "FIRST");
    CHECK(priorityName(PluginEventPriority::Last) == "LAST");
    CHECK(priorityOf("early") == PluginEventPriority::Early);
    CHECK(priorityOf("NORMAL") == PluginEventPriority::Normal);
    // `MONITOR` is not a priority, it is an observer, so there is nothing here to spell it.
    CHECK_FALSE(priorityOf("monitor").has_value());
}
