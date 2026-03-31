#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/poi/PoiManager.hpp>

#include <doctest.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::poi::BlockPos;
using basalt::server::runtime::world::poi::maskOf;
using basalt::server::runtime::world::poi::Occupancy;
using basalt::server::runtime::world::poi::PoiManager;
using basalt::server::runtime::world::poi::PoiRecord;
using basalt::server::runtime::world::poi::PoiType;
using basalt::wire::common::Identifier;
namespace poiTypes = basalt::server::runtime::world::poi::poiTypes;
namespace definitions = basalt::server::registry::blockStateDefinitions;

namespace {

[[nodiscard]] std::int32_t stateOf(std::string_view block, const std::map<std::string, std::string>& properties = {}) {
    const std::optional<std::int32_t> state =
        definitions::stateIdOf(Identifier::vanilla(std::string{block}), properties);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

} // namespace

TEST_CASE("only the head of a bed and every state of a workstation are points of interest") {
    CHECK(poiTypes::forState(stateOf("red_bed", {{"part", "head"}})) == PoiType::kHome);
    CHECK(poiTypes::forState(stateOf("red_bed", {{"part", "foot"}})) == PoiType::kNone);
    CHECK(poiTypes::forState(stateOf("water_cauldron", {{"level", "2"}})) == PoiType::kLeatherworker);
    CHECK(poiTypes::forState(stateOf("bell")) == PoiType::kMeeting);
    CHECK(poiTypes::maxTickets(PoiType::kMeeting) == 32);
    CHECK(poiTypes::validRange(PoiType::kMeeting) == 6);
    CHECK(poiTypes::jobSiteOf("minecraft:librarian") == PoiType::kLibrarian);
    CHECK(poiTypes::jobSiteOf("nitwit") == PoiType::kNone);
}

TEST_CASE("a claimed village point makes the sections around it a village") {
    PoiManager manager;
    const BlockPos bed{8, 64, 8};
    manager.noteBlockChange(bed, 0, stateOf("white_bed", {{"part", "head"}}));
    CHECK(manager.type(bed) == PoiType::kHome);
    // parity: an unclaimed bed is not a village centre.
    CHECK(manager.sectionsToVillage(0, 4, 0) == 7);

    REQUIRE(manager.acquire(bed));
    CHECK_FALSE(manager.acquire(bed));
    CHECK(manager.sectionsToVillage(0, 4, 0) == 0);
    CHECK(manager.sectionsToVillage(6, 4, -6) == 6);
    CHECK(manager.sectionsToVillage(7, 4, 0) == 7);
    CHECK(manager.isVillage(BlockPos{20, 70, 20}));
    CHECK_FALSE(manager.isVillage(BlockPos{40, 64, 8}));

    REQUIRE(manager.release(bed));
    CHECK(manager.sectionsToVillage(0, 4, 0) == 7);

    manager.noteBlockChange(bed, stateOf("white_bed", {{"part", "head"}}), 0);
    CHECK_FALSE(manager.type(bed).has_value());
}

TEST_CASE("queries walk only the types asked for and honour occupancy and the round radius") {
    PoiManager manager;
    manager.noteBlockChange(BlockPos{0, 64, 0}, 0, stateOf("composter"));
    manager.noteBlockChange(BlockPos{3, 64, 0}, 0, stateOf("lectern"));
    manager.noteBlockChange(BlockPos{40, 64, 40}, 0, stateOf("lectern"));
    std::vector<PoiRecord> found;
    manager.inRange(poiTypes::kAcquirableJobSites, BlockPos{0, 64, 0}, 48, Occupancy::kHasSpace, found);
    CHECK(found.size() == 2);
    manager.inSquare(maskOf(PoiType::kLibrarian), BlockPos{0, 64, 0}, 48, Occupancy::kAny, found);
    CHECK(found.size() == 2);
    REQUIRE(manager.acquire(BlockPos{3, 64, 0}));
    CHECK(manager.countInRange(poiTypes::kAcquirableJobSites, BlockPos{0, 64, 0}, 48, Occupancy::kHasSpace) == 1);
    CHECK(manager.findClosest(maskOf(PoiType::kLibrarian), BlockPos{0, 64, 0}, 48, Occupancy::kAny) ==
          BlockPos{3, 64, 0});
}

TEST_CASE("points of interest and their tickets survive a save") {
    const std::string directory = scratch("poi-save");
    {
        PoiManager manager;
        manager.configure(directory, basalt::server::world::kOverworldHeight);
        manager.noteBlockChange(BlockPos{-20, 70, 5}, 0, stateOf("bell"));
        REQUIRE(manager.acquire(BlockPos{-20, 70, 5}));
        manager.save();
    }
    PoiManager reloaded;
    reloaded.configure(directory, basalt::server::world::kOverworldHeight);
    const std::optional<PoiRecord> bell = reloaded.record(BlockPos{-20, 70, 5});
    REQUIRE(bell.has_value());
    CHECK(bell->type == PoiType::kMeeting);
    CHECK(bell->freeTickets == 31);
    CHECK(reloaded.isVillage(BlockPos{-20, 70, 5}));
}
