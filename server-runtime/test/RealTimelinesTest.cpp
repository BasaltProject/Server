#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <doctest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::RealTimelines;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace {

/// A jar that is a map of names to text. It lists entries in reverse order on purpose: the registry
/// has to sort them itself, because the index of an entry is the id the wire carries.
struct FakeJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] RealTimelines::Jar handle() {
        return RealTimelines::Jar{
            .readText = [this](std::string_view name) -> std::optional<std::string> {
                const auto found = files.find(std::string{name});
                return found == files.end() ? std::nullopt : std::optional{found->second};
            },
            .entriesUnder =
                [this](std::string_view prefix) {
                    std::vector<std::string> names;
                    for (const auto& [name, text] : files) {
                        if (name.starts_with(prefix)) {
                            names.push_back(name);
                        }
                    }
                    std::ranges::reverse(names);
                    return names;
                },
        };
    }
};

[[nodiscard]] std::string entry(const std::string& path) { return "data/minecraft/timeline/" + path + ".json"; }

/// Every log line this registry writes, so the reason an operator is given can be asserted.
struct CapturedLog {
    std::vector<std::string> lines;

    CapturedLog() {
        basalt::wire::common::logOutput::addSink([this](std::string_view line) { lines.emplace_back(line); });
    }

    CapturedLog(const CapturedLog&) = delete;
    CapturedLog& operator=(const CapturedLog&) = delete;

    ~CapturedLog() { basalt::wire::common::logOutput::clearSinks(); }
};

[[nodiscard]] bool mentions(const std::vector<std::string>& lines, std::string_view text) {
    return std::ranges::any_of(lines, [text](const std::string& line) { return line.find(text) != std::string::npos; });
}

} // namespace

TEST_CASE("the timelines the jar ships arrive sorted by name") {
    FakeJar jar{.files = {{entry("overworld"), R"({"tracks": {}})"},
                          {entry("end"), R"({"tracks": {}})"},
                          {entry("nether"), R"({"tracks": {}})"},
                          {"data/minecraft/dialog/quit.json", R"({})"}}};

    const RealTimelines timelines{jar.handle()};

    CHECK(timelines.size() == 3);
    CHECK(timelines.ids() == std::vector<Identifier>{Identifier::vanilla("end"), Identifier::vanilla("nether"),
                                                     Identifier::vanilla("overworld")});
    // Only what lives under the timeline prefix is a timeline.
    CHECK(timelines.definitionOf(Identifier::vanilla("quit")) == nullptr);
}

TEST_CASE("only the attributes a client is told about survive the trip") {
    FakeJar jar{.files = {{entry("overworld"), R"({
        "tracks": {
            "minecraft:visual/sky_color": {"type": "constant"},
            "visual/sun_angle": {"type": "constant"},
            "minecraft:gameplay/mob_spawning": {"type": "constant"},
            "visual/not_a_real_attribute": {"type": "constant"}
        },
        "loop_length": 24000
    })"}}};

    const RealTimelines timelines{jar.handle()};

    REQUIRE(timelines.size() == 1);
    const NbtCompound* definition = timelines.definitionOf(Identifier::vanilla("overworld"));
    REQUIRE(definition != nullptr);
    // parity: everything but `tracks` travels untouched.
    CHECK(definition->getInt("loop_length") == 24000);
    const NbtCompound* tracks = definition->getCompound("tracks");
    REQUIRE(tracks != nullptr);
    CHECK(tracks->size() == 2);
    CHECK(tracks->contains("minecraft:visual/sky_color"));
    CHECK(tracks->contains("visual/sun_angle"));
    CHECK_FALSE(tracks->contains("minecraft:gameplay/mob_spawning"));
    CHECK_FALSE(tracks->contains("visual/not_a_real_attribute"));
}

TEST_CASE("a timeline whose tracks are all unsyncable loses the key outright") {
    FakeJar jar{.files = {{entry("overworld"), R"({"tracks": {"minecraft:gameplay/mob_spawning": {}}, "x": 1})"}}};

    const RealTimelines timelines{jar.handle()};

    REQUIRE(timelines.size() == 1);
    const NbtCompound* definition = timelines.definitionOf(Identifier::vanilla("overworld"));
    REQUIRE(definition != nullptr);
    CHECK_FALSE(definition->contains("tracks"));
    CHECK(definition->getInt("x") == 1);
}

TEST_CASE("a timeline with no tracks at all is sent as it is") {
    FakeJar jar{.files = {{entry("overworld"), R"({"loop_length": 10})"}}};

    const RealTimelines timelines{jar.handle()};

    REQUIRE(timelines.size() == 1);
    const NbtCompound* definition = timelines.definitionOf(Identifier::vanilla("overworld"));
    REQUIRE(definition != nullptr);
    CHECK(definition->getInt("loop_length") == 10);
    CHECK_FALSE(definition->contains("tracks"));
}

TEST_CASE("one timeline that cannot be read costs the whole registry") {
    const CapturedLog log;
    FakeJar jar{.files = {{entry("end"), R"({"tracks": {}})"}, {entry("nether"), "{not json"}}};

    const RealTimelines timelines{jar.handle()};

    CHECK(timelines.size() == 0);
    CHECK(timelines.ids().empty());
    CHECK(mentions(log.lines, "Timeline 'nether'"));
    CHECK(mentions(log.lines, "malformed JSON"));
}

TEST_CASE("a timeline that is not an object is refused with the reason for it") {
    const CapturedLog log;
    FakeJar jar{.files = {{entry("overworld"), R"([1, 2])"}}};

    const RealTimelines timelines{jar.handle()};

    CHECK(timelines.size() == 0);
    CHECK(mentions(log.lines, "not a compound"));
}

TEST_CASE("a jar that lists an entry it cannot read is refused too") {
    const CapturedLog log;
    FakeJar jar;
    const RealTimelines::Jar half{
        .readText = [](std::string_view) -> std::optional<std::string> { return std::nullopt; },
        .entriesUnder = [](std::string_view) { return std::vector<std::string>{"data/minecraft/timeline/x.json"}; }};

    const RealTimelines timelines{half};

    CHECK(timelines.size() == 0);
    CHECK(mentions(log.lines, "no such entry"));
}

TEST_CASE("the two shapes NBT has no room for are named separately") {
    const CapturedLog nulls;
    FakeJar withNull{.files = {{entry("overworld"), R"({"tracks": null})"}}};
    CHECK(RealTimelines{withNull.handle()}.size() == 0);
    CHECK(mentions(nulls.lines, "a null has no NBT equivalent"));

    const CapturedLog mixed;
    FakeJar withMixedList{.files = {{entry("overworld"), R"({"tracks": [1, "two"]})"}}};
    CHECK(RealTimelines{withMixedList.handle()}.size() == 0);
    CHECK(mentions(mixed.lines, "a list mixes types"));
}

TEST_CASE("no jar at all is an empty registry rather than a crash") {
    const RealTimelines timelines{RealTimelines::Jar{}};

    CHECK(timelines.size() == 0);
    CHECK(timelines.entries().empty());
    CHECK(timelines.definitionOf(Identifier::vanilla("overworld")) == nullptr);

    // Half a jar is no jar either, whichever half is missing.
    FakeJar jar{.files = {{entry("overworld"), R"({"tracks": {}})"}}};
    const RealTimelines::Jar listing = jar.handle();
    CHECK(RealTimelines{RealTimelines::Jar{.readText = listing.readText}}.size() == 0);
    CHECK(RealTimelines{RealTimelines::Jar{.entriesUnder = listing.entriesUnder}}.size() == 0);
}
