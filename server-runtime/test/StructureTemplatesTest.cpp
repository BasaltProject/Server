#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

using basalt::server::runtime::world::StructureTemplates;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;

namespace {

/// The smallest structure file vanilla writes: a size, one palette entry and one block using it.
[[nodiscard]] std::vector<std::uint8_t> structureBytes(const std::string& block) {
    NbtList size;
    size.push(NbtTag::ofInt(1));
    size.push(NbtTag::ofInt(1));
    size.push(NbtTag::ofInt(1));

    NbtCompound state;
    state.putString("Name", block);
    NbtList palette;
    palette.push(NbtTag::ofCompound(state));

    NbtList position;
    position.push(NbtTag::ofInt(0));
    position.push(NbtTag::ofInt(0));
    position.push(NbtTag::ofInt(0));
    NbtCompound entry;
    entry.set("pos", NbtTag::ofList(position));
    entry.putInt("state", 0);
    NbtList blocks;
    blocks.push(NbtTag::ofCompound(entry));

    NbtCompound root;
    root.set("size", NbtTag::ofList(size));
    root.set("palette", NbtTag::ofList(palette));
    root.set("blocks", NbtTag::ofList(blocks));
    const std::expected<std::vector<std::uint8_t>, basalt::wire::io::CompressionError> gzipped =
        basalt::wire::io::gzip::compress(basalt::wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root))));
    REQUIRE(gzipped.has_value());
    return *gzipped;
}

struct FakeJar {
    std::map<std::string, std::vector<std::uint8_t>> files;
    std::map<std::string, std::vector<Identifier>> listings;
    std::int32_t reads = 0;
    std::int32_t walks = 0;

    [[nodiscard]] StructureTemplates::Jar handle() {
        return StructureTemplates::Jar{
            .readEntry = [this](std::string_view name) -> std::optional<std::vector<std::uint8_t>> {
                ++reads;
                const auto found = files.find(std::string{name});
                return found == files.end() ? std::nullopt : std::optional{found->second};
            },
            .dataIds =
                [this](std::string_view directory, std::string_view extension) {
                    ++walks;
                    static_cast<void>(extension);
                    const auto found = listings.find(std::string{directory});
                    return found == listings.end() ? std::vector<Identifier>{} : found->second;
                },
        };
    }
};

} // namespace

TEST_CASE("a structure is read from the jar under its own namespace") {
    FakeJar jar{
        .files = {{"data/minecraft/structure/village/plains/houses/small.nbt", structureBytes("minecraft:stone")},
                  {"data/mypack/structure/tower.nbt", structureBytes("minecraft:dirt")}}};
    StructureTemplates templates{jar.handle()};

    const auto* small = templates.find(Identifier::vanilla("village/plains/houses/small"));
    REQUIRE(small != nullptr);
    REQUIRE(small->blocks().size() == 1);
    CHECK(small->blocks()[0].stateId == basalt::server::registry::blocks::named("stone")->stateId);

    const auto* tower = templates.find(Identifier{"mypack", "tower"});
    REQUIRE(tower != nullptr);
    CHECK(tower->blocks()[0].stateId == basalt::server::registry::blocks::named("dirt")->stateId);
}

TEST_CASE("a structure the jar does not ship is looked for once and then remembered") {
    FakeJar jar;
    StructureTemplates templates{jar.handle()};

    CHECK(templates.find(Identifier::vanilla("missing")) == nullptr);
    CHECK(jar.reads == 1);
    CHECK(templates.find(Identifier::vanilla("missing")) == nullptr);
    CHECK(jar.reads == 1);
}

TEST_CASE("one that is read stays at the same address for the life of the cache") {
    FakeJar jar{.files = {{"data/minecraft/structure/a.nbt", structureBytes("minecraft:stone")},
                          {"data/minecraft/structure/b.nbt", structureBytes("minecraft:dirt")}}};
    StructureTemplates templates{jar.handle()};

    const auto* first = templates.find(Identifier::vanilla("a"));
    REQUIRE(first != nullptr);
    static_cast<void>(templates.find(Identifier::vanilla("b")));

    CHECK(templates.find(Identifier::vanilla("a")) == first);
    CHECK(jar.reads == 2);
}

TEST_CASE("a file that is not a gzipped compound is refused with the reason for it") {
    std::vector<std::string> lines;
    basalt::wire::common::logOutput::addSink([&lines](std::string_view line) { lines.emplace_back(line); });
    const std::expected<std::vector<std::uint8_t>, basalt::wire::io::CompressionError> notNbt =
        basalt::wire::io::gzip::compress(std::vector<std::uint8_t>{9, 9, 9});
    REQUIRE(notNbt.has_value());
    const std::expected<std::vector<std::uint8_t>, basalt::wire::io::CompressionError> notCompound =
        basalt::wire::io::gzip::compress(basalt::wire::nbt::nbtIO::write(NbtTag::ofInt(1)));
    REQUIRE(notCompound.has_value());
    FakeJar jar{.files = {{"data/minecraft/structure/plain.nbt", {1, 2, 3}},
                          {"data/minecraft/structure/garbage.nbt", *notNbt},
                          {"data/minecraft/structure/number.nbt", *notCompound}}};
    // The reason goes out at debug, which is off by default.
    basalt::wire::common::Logger logger{"Structures"};
    logger.setMinLevel(basalt::wire::common::LogLevel::Debug);
    StructureTemplates templates{jar.handle(), std::move(logger)};

    CHECK(templates.find(Identifier::vanilla("plain")) == nullptr);
    CHECK(templates.find(Identifier::vanilla("garbage")) == nullptr);
    CHECK(templates.find(Identifier::vanilla("number")) == nullptr);

    const auto says = [&lines](std::string_view text) {
        return std::ranges::any_of(lines,
                                   [text](const std::string& line) { return line.find(text) != std::string::npos; });
    };
    CHECK(says("not gzip"));
    CHECK(says("not NBT"));
    CHECK(says("not a compound"));
    basalt::wire::common::logOutput::clearSinks();
}

TEST_CASE("the two listings are walked once each and kept apart") {
    FakeJar jar{.listings = {{"structure", {Identifier::vanilla("a"), Identifier::vanilla("b")}},
                             {"worldgen/template_pool", {Identifier::vanilla("village/plains/town_centers")}}}};
    StructureTemplates templates{jar.handle()};

    CHECK(templates.templateIds() == std::vector<Identifier>{Identifier::vanilla("a"), Identifier::vanilla("b")});
    CHECK(templates.poolIds() == std::vector<Identifier>{Identifier::vanilla("village/plains/town_centers")});
    CHECK(jar.walks == 2);
    CHECK(templates.templateIds().size() == 2);
    CHECK(templates.poolIds().size() == 1);
    CHECK(jar.walks == 2);
}

TEST_CASE("no jar at all answers nothing rather than falling over") {
    StructureTemplates templates{StructureTemplates::Jar{}};

    CHECK(templates.find(Identifier::vanilla("a")) == nullptr);
    CHECK(templates.templateIds().empty());
    CHECK(templates.poolIds().empty());
}
