#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace basalt::server::plugins;
using namespace basalt::wire::lua;

namespace {
class FakeWorld final : public PluginWorldRef {
public:
    std::vector<std::int32_t> section = std::vector<std::int32_t>(4096, 5);
    std::vector<std::uint8_t> blockEntity;
    std::int32_t written = 0;

    [[nodiscard]] std::string name() const override { return "fake"; }

    [[nodiscard]] std::int64_t time() const override { return 0; }

    [[nodiscard]] bool raining() const override { return false; }

    [[nodiscard]] bool thundering() const override { return false; }

    [[nodiscard]] std::string difficulty() const override { return "normal"; }

    [[nodiscard]] std::optional<std::string> block(std::int32_t, std::int32_t, std::int32_t) const override {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> blockState(std::int32_t, std::int32_t, std::int32_t) const override {
        return std::nullopt;
    }

    bool setBlock(std::int32_t, std::int32_t, std::int32_t, std::string_view) override { return false; }

    [[nodiscard]] std::vector<std::shared_ptr<PluginEntityRef>> entities() const override { return {}; }

    std::shared_ptr<PluginHologramRef> hologram(std::string_view, double, double, double) override { return nullptr; }

    std::shared_ptr<PluginNpcRef> npc(std::string_view, double, double, double,
                                      std::function<void(const std::string&)>) override {
        return nullptr;
    }

    std::shared_ptr<PluginEntityRef> spawn(std::string_view, double, double, double) override { return nullptr; }

    [[nodiscard]] std::optional<std::string> gameRule(std::string_view) const override { return std::nullopt; }

    bool setGameRule(std::string_view, std::string_view) override { return false; }

    [[nodiscard]] std::vector<std::string> gameRules() const override { return {}; }

    bool readSection(std::int32_t, std::int32_t, std::int32_t, bool, std::span<std::int32_t> out) override {
        std::copy(section.begin(), section.end(), out.begin());
        return true;
    }

    bool writeSection(std::int32_t, std::int32_t, std::int32_t, std::span<const std::int32_t> states, bool) override {
        section.assign(states.begin(), states.end());
        written++;
        return true;
    }

    std::optional<std::vector<std::uint8_t>> blockEntityNbt(std::int32_t, std::int32_t, std::int32_t) override {
        return blockEntity;
    }

    bool setBlockEntityNbt(std::int32_t, std::int32_t, std::int32_t, std::span<const std::uint8_t> nbt) override {
        blockEntity.assign(nbt.begin(), nbt.end());
        return true;
    }
};
} // namespace

TEST_CASE("a plugin reads and writes sections and block entities through the world object") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    const auto world = std::make_shared<FakeWorld>();
    world->blockEntity = basalt::wire::nbt::nbtIO::write(*basalt::wire::nbt::snbt::parse("{id:\"minecraft:chest\"}"));

    const auto chunk = sandbox->compile(R"(
        local w = ...
        local s = w:section(0, 0, 0)
        assert(#s == 4096 and s[1] == 5)
        s[1] = 7
        assert(w:set_section(0, 0, 0, s))
        local be = w:block_entity(1, 2, 3)
        assert(be:find("chest"))
        assert(w:set_block_entity(1, 2, 3, "{id:\"minecraft:barrel\"}"))
        print("done")
    )",
                                        "bulk.lua");
    REQUIRE(chunk.has_value());
    REQUIRE(sandbox->bind(*chunk).has_value());
    LuaArgs lua{sandbox->state()};
    pushWorld(lua, world);
    std::optional<LuaFailure> failure;
    static_cast<void>(LuaCall::start(*sandbox, 1, [&](std::optional<LuaFailure> reported) { failure = reported; }));

    CHECK_FALSE(failure.has_value());
    CHECK(printed == std::vector<std::string>{"done"});
    CHECK(world->section[0] == 7);
    const auto stored = basalt::wire::nbt::nbtIO::read(world->blockEntity);
    REQUIRE(stored.has_value());
    CHECK(basalt::wire::nbt::snbt::stringify(stored->tag).find("barrel") != std::string::npos);
}
