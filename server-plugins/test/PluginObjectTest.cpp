#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/lua/LuaSandbox.hpp>
#include <basalt/server/plugins/PluginObject.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>

using namespace basalt::server::plugins;
using basalt::server::lua::LuaArgs;
using basalt::server::lua::LuaChunk;
using basalt::server::lua::LuaFailure;
using basalt::server::lua::LuaSandbox;

namespace {

/// Runs one chunk and answers what went wrong, or nothing.
[[nodiscard]] std::optional<LuaFailure> run(LuaSandbox& sandbox, std::string_view source) {
    const std::expected<LuaChunk, LuaFailure> chunk = sandbox.compile(source, "test.lua");
    if (!chunk.has_value()) {
        return chunk.error();
    }
    const std::expected<void, LuaFailure> result = sandbox.run(*chunk);
    return result.has_value() ? std::nullopt : std::optional{result.error()};
}

/// A sandbox with one object called `thing` standing in its globals.
struct Fixture {
    std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("objects");
    double health = 20.0;
    std::string lastSent;

    Fixture() {
        basalt::server::lua::installHostFunction(*sandbox, "makeThing", [this](LuaArgs& lua) {
            PluginObject object{"thing"};
            object.reads("name", [] { return PluginValue{std::string{"steve"}}; })
                .reads("health", [this] { return PluginValue{health}; })
                .writes("health", [this](LuaArgs& args, std::int32_t index) { health = args.checkNumber(index); })
                .method("send", [this](LuaArgs& args) {
                    lastSent = args.checkString(firstArgument(args));
                    return 0;
                });
            std::move(object).push(lua);
            return 1;
        });
        static_cast<void>(run(*sandbox, "thing = makeThing()"));
    }
};

} // namespace

TEST_CASE("an object reads and writes the thing behind it") {
    Fixture fixture;

    REQUIRE(!run(*fixture.sandbox, "thing.health = thing.health + 5").has_value());
    CHECK(fixture.health == doctest::Approx(25.0));

    // Both call shapes reach a method, because a plugin should not have to care which it wrote.
    REQUIRE(!run(*fixture.sandbox, "thing:send('hello')").has_value());
    CHECK(fixture.lastSent == "hello");
    REQUIRE(!run(*fixture.sandbox, "thing.send('again')").has_value());
    CHECK(fixture.lastSent == "again");
}

TEST_CASE("reading a name an object does not have says what it does have") {
    Fixture fixture;

    const std::optional<LuaFailure> missing = run(*fixture.sandbox, "return thing.nope");
    REQUIRE(missing.has_value());
    CHECK(missing->message.find("a thing has no 'nope'") != std::string::npos);
    // The list is what turns a typo into a one-line fix.
    CHECK(missing->message.find("health") != std::string::npos);
    CHECK(missing->message.find("send") != std::string::npos);

    // A field that is there but read-only says so, rather than pretending the write landed.
    const std::optional<LuaFailure> readOnly = run(*fixture.sandbox, "thing.name = 'alex'");
    REQUIRE(readOnly.has_value());
    CHECK(readOnly->message.find("can be read but not changed") != std::string::npos);
}
