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
                if (name != "player") {
                    return false;
                }
                lua.pushString("a player object");
                return true;
            },
            [](std::string_view message) { return "resolved:" + std::string{message}; });
    }

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

    Fixture join{joinType()};
    const std::optional<LuaFailure> none =
        join.fire(R"(local e = ... e.world = "nether")", {std::string{"Whiron"}, std::string{"world"}});
    REQUIRE(none.has_value());
    CHECK(none->message.find("it lets you change nothing") != std::string::npos);
}

TEST_CASE("an event kept past its handler refuses to answer") {
    Fixture fixture{breakType()};
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

    const std::expected<LuaChunk, LuaFailure> written =
        fixture.sandbox().compile("stashed.block = 'minecraft:dirt'", "written.lua");
    REQUIRE(written.has_value());
    const std::expected<void, LuaFailure> refused = fixture.sandbox().run(*written);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().message.find("event is over") != std::string::npos);
}
