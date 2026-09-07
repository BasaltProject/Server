#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginEventView.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::plugins;
using namespace basalt::server::lua;

namespace {

[[nodiscard]] const PluginEventType& breakType() {
    static const PluginEventType type{
        "block.break", true, {"player_name", "player_uuid", "world", "x", "y", "z", "block"}, {"block"}};
    return type;
}

[[nodiscard]] const PluginEventType& joinType() {
    static const PluginEventType type{"player.join", false, {"player_name", "world"}};
    return type;
}

/// Carries the halves of the pairs the hints are keyed on, and not the halves that trigger them: an
/// event with `player_uuid` but no `player_name` has no player object to offer, and one with `z` but
/// no `x` has no position.
[[nodiscard]] const PluginEventType& hintlessType() {
    static const PluginEventType type{"world.tick", true, {"player_uuid", "z", "amount"}, {"amount"}};
    return type;
}

/// A bus, an event filled in, a sandbox and the view over it: what one handler is handed.
class Fixture {
public:
    explicit Fixture(const PluginEventType& type) : type_(&type) {
        bus_.declare(type);
        sandbox_ =
            LuaSandbox::create("arena", {}, 0, {}, [this](std::string_view line) { printed_.emplace_back(line); });
        view_ = PluginEventView::create(
            *sandbox_, bus_.event(type),
            [this](std::optional<std::string> message) {
                cancelled_ = true;
                cancelMessage_ = std::move(message);
            },
            [](LuaArgs& lua, std::string_view name) {
                // The one object this fixture knows how to build, which is enough to show that a
                // name the event does not carry can still answer.
                if (name != "player") {
                    return false;
                }
                lua.pushString("a player object");
                return true;
            },
            [](std::string_view message) { return "resolved:" + std::string{message}; });
    }

    /// Fires the event with the fields filled in, running `source` as the only handler.
    [[nodiscard]] std::optional<LuaFailure> fire(std::string_view source, const std::vector<PluginEventValue>& values) {
        PluginEvent& event = bus_.event(*type_);
        std::optional<LuaFailure> failure;
        const PluginHandle handle =
            bus_.add(*type_, "arena", 0, PluginEventPriority::Normal, false, false, {}, [&](PluginEvent&) {
                view_->bind();
                const std::expected<LuaChunk, LuaFailure> chunk = sandbox_->compile(source, "handler.lua");
                if (!chunk.has_value()) {
                    failure = chunk.error();
                    return;
                }
                if (!sandbox_->bind(*chunk).has_value()) {
                    return;
                }
                LuaArgs lua{sandbox_->state()};
                view_->push(lua);
                static_cast<void>(LuaCall::start(
                    *sandbox_, 1, [&](std::optional<LuaFailure> reported) { failure = std::move(reported); }));
            });
        for (std::size_t index = 0; index < values.size(); ++index) {
            event.set(static_cast<std::int32_t>(index), values[index]);
        }
        allowed_ = bus_.fire(event);
        return failure;
    }

    [[nodiscard]] const std::vector<std::string>& printed() const noexcept { return printed_; }

    [[nodiscard]] bool allowed() const noexcept { return allowed_; }

    [[nodiscard]] bool cancelled() const noexcept { return cancelled_; }

    [[nodiscard]] const std::optional<std::string>& cancelMessage() const noexcept { return cancelMessage_; }

    [[nodiscard]] PluginEvent& event() { return bus_.event(*type_); }

    [[nodiscard]] PluginEventView& view() { return *view_; }

    [[nodiscard]] LuaSandbox& sandbox() { return *sandbox_; }

private:
    const PluginEventType* type_;
    PluginEventBus bus_;
    std::unique_ptr<LuaSandbox> sandbox_;
    std::unique_ptr<PluginEventView> view_;
    std::vector<std::string> printed_;
    bool allowed_ = true;
    bool cancelled_ = false;
    std::optional<std::string> cancelMessage_;
};

[[nodiscard]] std::vector<PluginEventValue> stoneBreak() {
    return {std::string{"Whiron"}, std::string{"uuid-1"}, std::string{"world"},          std::int64_t{1},
            std::int64_t{64},      std::int64_t{-3},      std::string{"minecraft:stone"}};
}

} // namespace

TEST_CASE("a handler reads the event through the view rather than a copy") {
    Fixture fixture{breakType()};
    const std::optional<LuaFailure> failure = fixture.fire(R"(
        local e = ...
        print(e.name, e.player_name, e.world, e.block)
        print(e.x, e.y, e.z, math.type(e.x))
        print(e.cancelled)
        -- A name the event does not carry, answered by the host rather than by a field.
        print(e.player)
    )",
                                                           stoneBreak());
    REQUIRE_FALSE(failure.has_value());
    REQUIRE(fixture.printed().size() == 4);
    CHECK(fixture.printed()[0] == "block.break\tWhiron\tworld\tminecraft:stone");
    // The numbers keep their kind on the way through, which is what a coordinate needs.
    CHECK(fixture.printed()[1] == "1\t64\t-3\tinteger");
    CHECK(fixture.printed()[2] == "false");
    CHECK(fixture.printed()[3] == "a player object");
}

TEST_CASE("what a handler writes reaches the event, and what it may not write is refused") {
    Fixture fixture{breakType()};
    const std::optional<LuaFailure> written = fixture.fire(R"(
        local e = ...
        e.block = "minecraft:cobblestone"
    )",
                                                           stoneBreak());
    REQUIRE_FALSE(written.has_value());
    CHECK(std::get<std::string>(fixture.event().get("block")) == "minecraft:cobblestone");

    Fixture refused{breakType()};
    const std::optional<LuaFailure> failure = refused.fire(R"(
        local e = ...
        e.world = "nether"
    )",
                                                           stoneBreak());
    REQUIRE(failure.has_value());
    CHECK(failure->message.find("'world' cannot be changed on a block.break event; it lets you change block") !=
          std::string::npos);

    // An event that declares nothing mutable says so rather than naming an empty list.
    Fixture join{joinType()};
    const std::optional<LuaFailure> none =
        join.fire(R"(local e = ... e.world = "nether")", {std::string{"Whiron"}, std::string{"world"}});
    REQUIRE(none.has_value());
    CHECK(none->message.find("it lets you change nothing") != std::string::npos);
}

TEST_CASE("a field the event does not have names the ones it does") {
    Fixture fixture{breakType()};
    const std::optional<LuaFailure> failure = fixture.fire(R"(local e = ... print(e.nope))", stoneBreak());
    REQUIRE(failure.has_value());
    CHECK(failure->message.find("a block.break event has no 'nope'; it has player_name, player_uuid, world, x, y, "
                                "z, block (and 'player') (and 'pos')") != std::string::npos);
}

TEST_CASE("the hints only appear for the fields that have something behind them") {
    Fixture fixture{hintlessType()};
    const std::optional<LuaFailure> failure =
        fixture.fire(R"(local e = ... print(e.nope))", {std::string{"uuid-1"}, std::int64_t{-3}, std::int64_t{5}});
    REQUIRE(failure.has_value());
    CHECK(failure->message.find("a world.tick event has no 'nope'; it has player_uuid, z, amount") !=
          std::string::npos);
    CHECK(failure->message.find("(and 'player')") == std::string::npos);
    CHECK(failure->message.find("(and 'pos')") == std::string::npos);

    // A number written to a mutable field stays a number of the same kind.
    Fixture written{hintlessType()};
    REQUIRE_FALSE(
        written.fire(R"(local e = ... e.amount = 42)", {std::string{"uuid-1"}, std::int64_t{-3}, std::int64_t{5}})
            .has_value());
    CHECK(std::get<std::int64_t>(written.event().get("amount")) == 42);
    Fixture real{hintlessType()};
    REQUIRE_FALSE(
        real.fire(R"(local e = ... e.amount = 1.5)", {std::string{"uuid-1"}, std::int64_t{-3}, std::int64_t{5}})
            .has_value());
    CHECK(std::get<double>(real.event().get("amount")) == 1.5);
}

TEST_CASE("cancelling, with a message and without") {
    Fixture bare{breakType()};
    REQUIRE_FALSE(bare.fire(R"(local e = ... e:cancel())", stoneBreak()).has_value());
    CHECK(bare.cancelled());
    CHECK_FALSE(bare.cancelMessage().has_value());

    Fixture spoken{breakType()};
    REQUIRE_FALSE(spoken.fire(R"(local e = ... e:cancel("not here"))", stoneBreak()).has_value());
    CHECK(spoken.cancelled());
    // A refusal is text like any other, so it went through the resolver on the way out.
    CHECK(spoken.cancelMessage() == "resolved:not here");

    // A number is text as far as Lua is concerned, and the frozen Kotlin's `isstring` says the same,
    // so a reason written as a number is still a reason.
    Fixture numbered{breakType()};
    REQUIRE_FALSE(numbered.fire(R"(local e = ... e:cancel(42))", stoneBreak()).has_value());
    CHECK(numbered.cancelled());
    CHECK(numbered.cancelMessage() == "resolved:42");

    // A boolean is not, and asking for it as text would raise inside the cancel rather than
    // cancelling: it is left without a reason instead.
    Fixture flagged{breakType()};
    REQUIRE_FALSE(flagged.fire(R"(local e = ... e:cancel(true))", stoneBreak()).has_value());
    CHECK(flagged.cancelled());
    CHECK_FALSE(flagged.cancelMessage().has_value());

    // Called with a dot rather than a colon, which is the same thing from the plugin's side.
    Fixture dotted{breakType()};
    REQUIRE_FALSE(dotted.fire(R"(local e = ... e.cancel("dotted"))", stoneBreak()).has_value());
    CHECK(dotted.cancelMessage() == "resolved:dotted");

    // An event that is telling rather than asking cannot be cancelled at all.
    Fixture join{joinType()};
    const std::optional<LuaFailure> failure =
        join.fire(R"(local e = ... e:cancel())", {std::string{"Whiron"}, std::string{"world"}});
    REQUIRE(failure.has_value());
    CHECK(failure->message.find("a player.join event cannot be cancelled: it is telling you, not asking") !=
          std::string::npos);
    CHECK_FALSE(join.cancelled());
}

TEST_CASE("an event kept past its handler refuses to answer") {
    Fixture fixture{breakType()};
    // The handler stashes the view in a global, which is the mistake this guards against: the event
    // object is reused, so reading it a tick later would be reading somebody else's.
    REQUIRE_FALSE(fixture.fire(R"(stashed = ...)", stoneBreak()).has_value());

    const std::expected<LuaChunk, LuaFailure> chunk = fixture.sandbox().compile("return stashed.block", "later.lua");
    REQUIRE(chunk.has_value());
    const std::expected<void, LuaFailure> result = fixture.sandbox().run(*chunk);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().message.find(
              "this block.break event is over: it cannot be read or changed after the handler returns") !=
          std::string::npos);

    // Whether it was cancelled is about the event too, so that is refused as well.
    const std::expected<LuaChunk, LuaFailure> asked =
        fixture.sandbox().compile("return stashed.cancelled", "asked.lua");
    REQUIRE(asked.has_value());
    const std::expected<void, LuaFailure> refusedRead = fixture.sandbox().run(*asked);
    REQUIRE_FALSE(refusedRead.has_value());
    CHECK(refusedRead.error().message.find("event is over") != std::string::npos);

    // Its name still answers, because a name is not a field of the event that moved on.
    const std::expected<LuaChunk, LuaFailure> named =
        fixture.sandbox().compile("assert(stashed.name == 'block.break')", "named.lua");
    REQUIRE(named.has_value());
    CHECK(fixture.sandbox().run(*named).has_value());

    // And writing to it is refused too, not quietly kept.
    const std::expected<LuaChunk, LuaFailure> written =
        fixture.sandbox().compile("stashed.block = 'minecraft:dirt'", "written.lua");
    REQUIRE(written.has_value());
    const std::expected<void, LuaFailure> refused = fixture.sandbox().run(*written);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().message.find("event is over") != std::string::npos);
}
