#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginToml.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::server::plugins;
using namespace basalt::server::lua;
namespace toml = basalt::wire::common::toml;

namespace {

/// Runs a chunk with the parsed config behind a `config()` host function.
[[nodiscard]] std::vector<std::string> configured(std::string_view configText, std::string_view source) {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);

    const std::expected<toml::TomlTable, toml::TomlError> table = toml::parse(configText);
    REQUIRE(table.has_value());
    // The host function is how a plugin asks for its config, so the bridge is exercised the way it
    // really runs rather than through a stack the test poked at.
    installHostFunction(*sandbox, "config", [&](LuaArgs& args) {
        pluginToml::push(args, *table);
        return 1;
    });

    const std::expected<LuaChunk, LuaFailure> chunk = sandbox->compile(source, "test.lua");
    REQUIRE(chunk.has_value());
    const std::expected<void, LuaFailure> result = sandbox->run(*chunk);
    if (!result.has_value()) {
        printed.push_back("FAILED: " + result.error().message);
    }
    return printed;
}

} // namespace

TEST_CASE("a config reaches lua with the types it was written with") {
    const std::vector<std::string> printed = configured(R"(
name = "Arena"
players = 24
rate = 1.5
enabled = true
worlds = ["arena", "lobby"]

[rewards]
first = 100
items = ["diamond", "emerald"]

[rewards.second]
coins = 50
)",
                                                        R"(
        local c = config()
        print(c.name, type(c.name))
        -- An integer written as an integer stays one, which is what `%d` and array indices need.
        print(c.players, math.type(c.players))
        print(c.rate, math.type(c.rate))
        print(c.enabled, type(c.enabled))
        print(#c.worlds, c.worlds[1], c.worlds[2])
        print(c.rewards.first, math.type(c.rewards.first))
        print(#c.rewards.items, c.rewards.items[1])
        print(c.rewards.second.coins)
        print(c.missing)
    )");

    REQUIRE(printed.size() == 9);
    CHECK(printed[0] == "Arena\tstring");
    CHECK(printed[1] == "24\tinteger");
    CHECK(printed[2] == "1.5\tfloat");
    CHECK(printed[3] == "true\tboolean");
    CHECK(printed[4] == "2\tarena\tlobby");
    CHECK(printed[5] == "100\tinteger");
    CHECK(printed[6] == "2\tdiamond");
    CHECK(printed[7] == "50");
    CHECK(printed[8] == "nil");
}

TEST_CASE("a kebab-case key answers to an underscore as well") {
    const std::vector<std::string> printed = configured(R"(
starting-balance = 500
plain = 1

[shop-settings]
max-items = 9
)",
                                                        R"(
        local c = config()
        -- Lua cannot spell a dash in `c.starting-balance`, so both names reach the same value.
        print(c.starting_balance, c["starting-balance"])
        print(c.shop_settings.max_items, c["shop-settings"]["max-items"])
        -- A key with no dash is not duplicated: there is nothing to spell differently.
        print(c.plain)
        local names = {}
        for key in pairs(c) do names[#names + 1] = key end
        table.sort(names)
        print(table.concat(names, ","))
    )");

    REQUIRE(printed.size() == 4);
    CHECK(printed[0] == "500\t500");
    CHECK(printed[1] == "9\t9");
    CHECK(printed[2] == "1");
    CHECK(printed[3] == "plain,shop-settings,shop_settings,starting-balance,starting_balance");
}

TEST_CASE("an empty config is an empty table, not a missing one") {
    const std::vector<std::string> printed = configured("", R"(
        local c = config()
        print(type(c), next(c) == nil)
    )");
    REQUIRE(printed.size() == 1);
    CHECK(printed[0] == "table\ttrue");
}

TEST_CASE("an array of arrays keeps its shape") {
    const std::vector<std::string> printed = configured(R"(
empty = []
mixed = [1, "two", true, 4.5]
)",
                                                        R"(
        local c = config()
        print(#c.empty, type(c.empty))
        print(#c.mixed, c.mixed[1], c.mixed[2], c.mixed[3], c.mixed[4])
        print(math.type(c.mixed[1]), math.type(c.mixed[4]))
    )");
    REQUIRE(printed.size() == 3);
    CHECK(printed[0] == "0\ttable");
    CHECK(printed[1] == "4\t1\ttwo\ttrue\t4.5");
    CHECK(printed[2] == "integer\tfloat");
}
