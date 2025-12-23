#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/ServerMetrics.hpp>
#include <basalt/server/runtime/SubstituteLabels.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::ChunkSimulationView;
using basalt::server::runtime::CpuMeter;
using basalt::server::runtime::ServerMetrics;
using basalt::server::runtime::SubstituteLabels;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Uuid;
namespace config = basalt::server::runtime::config;
namespace reg = basalt::server::registry;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

} // namespace

TEST_CASE("a substitute is named after its own id, with the version that introduced it") {
    const SubstituteLabels labels{"26.2"};
    const reg::Item* stone = reg::items::registry().byName(basalt::wire::common::Identifier::vanilla("stone"));
    REQUIRE(stone != nullptr);

    const std::optional<basalt::wire::text::Component> named = labels.itemLabel(stone->protocolId);
    REQUIRE(named.has_value());
    CHECK(basalt::wire::text::toPlainText(*named) == "Stone (26.2)");

    // Every underscore becomes a space and every word is capitalised: the client has no translation
    // for something it does not know, so the name has to read as one already.
    const reg::Item* sword = reg::items::registry().byName(basalt::wire::common::Identifier::vanilla("diamond_sword"));
    REQUIRE(sword != nullptr);
    CHECK(basalt::wire::text::toPlainText(*labels.itemLabel(sword->protocolId)) == "Diamond Sword (26.2)");

    // An id no registry knows has no name to give, which leaves the substitute unnamed rather than
    // named after nothing.
    CHECK_FALSE(labels.itemLabel(-1).has_value());
    CHECK_FALSE(labels.entityLabel(-1).has_value());

    const reg::EntityTypeEntry* pig = reg::entityTypes::of(basalt::wire::common::Identifier::vanilla("pig"));
    REQUIRE(pig != nullptr);
    CHECK(basalt::wire::text::toPlainText(*labels.entityLabel(pig->protocolId)) == "Pig (26.2)");
}

TEST_CASE("the simulated chunks are each player's square, deduplicated and capped at vanilla's distance") {
    PlayerRegistry registry;
    const std::string root = scratch("simulation-view");
    LoadedWorld world{config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
                      config::WorldSettings{.viewDistance = 32}, WorldMetadata{}, root,
                      basalt::wire::common::Logger{"Test"}};
    ChunkSimulationView view{registry};

    CHECK(view.simulatedChunks(world).empty());

    PlayerSession steve{Uuid{1, 1}, "Steve", 1, PlayerConnection{}};
    steve.worldName = "world";
    registry.add(&steve);
    // 32 was asked for but vanilla simulates ten, so the square is 21 by 21 and not 65 by 65.
    CHECK(view.simulatedChunks(world).size() == 21 * 21);

    // A second player one chunk away shares almost all of that square, and no chunk is listed twice.
    PlayerSession alex{Uuid{2, 1}, "Alex", 2, PlayerConnection{}};
    alex.worldName = "world";
    alex.x = 16.0;
    registry.add(&alex);
    const std::vector<std::array<std::int32_t, 2>> both = view.simulatedChunks(world);
    CHECK(both.size() == 21 * 21 + 21);
    std::vector<std::array<std::int32_t, 2>> sorted = both;
    std::ranges::sort(sorted);
    CHECK(std::ranges::adjacent_find(sorted) == sorted.end());

    // Someone in another world is not in this one's square at all.
    alex.worldName = "nether";
    CHECK(view.simulatedChunks(world).size() == 21 * 21);
}

TEST_CASE("the cpu meter needs two readings, and remembers the last answer between windows") {
    CpuMeter meter;
    // The first reading is a baseline: there is no percentage of one instant.
    CHECK_FALSE(meter.percent().has_value());

    meter.prime();
    // Within the first short window the meter answers something rather than nothing, and keeps
    // answering the same thing until the window is up.
    const std::optional<double> first = meter.percent();
    if (first.has_value()) {
        CHECK(*first >= 0.0);
        CHECK(meter.percent() == first);
    }
}

TEST_CASE("the memory report says what the process holds, or says the platform will not tell") {
    ServerMetrics metrics;
    const std::optional<std::uint64_t> resident = metrics.residentBytes();
    if (resident.has_value()) {
        CHECK(*resident > 0);
        // The peak is a high-water mark, so it never reads below the reading that set it.
        CHECK(metrics.peakResidentBytes() >= *resident);
        CHECK(metrics.memoryReport().find("MB resident") != std::string::npos);
    } else {
        CHECK(metrics.memoryReport().find("does not report") != std::string::npos);
    }
}
