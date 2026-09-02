#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <doctest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::RealDialogs;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace {

/// A jar that is a map of names to text. It lists entries in reverse order on purpose: the registry
/// has to sort them itself, because the index of an entry is the id the wire carries.
struct FakeJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] RealDialogs::Jar handle() {
        return RealDialogs::Jar{
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

[[nodiscard]] std::string entry(const std::string& path) { return "data/minecraft/dialog/" + path + ".json"; }

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

} // namespace

TEST_CASE("every dialog the jar ships is an entry, sorted, and its index is its protocol id") {
    FakeJar jar;
    jar.files[entry("server_links")] = R"({"type":"minecraft:server_links"})";
    jar.files[entry("custom_options")] = R"({"type":"minecraft:multi_action"})";
    jar.files[entry("quick_actions")] = R"({"type":"minecraft:notice"})";
    jar.files["data/minecraft/dialog/server_links.mcmeta"] = "{}";
    const RealDialogs dialogs{jar.handle()};

    REQUIRE(dialogs.size() == 3);
    CHECK(dialogs.entries()[0].first == Identifier::vanilla("custom_options"));
    CHECK(dialogs.entries()[1].first == Identifier::vanilla("quick_actions"));
    CHECK(dialogs.entries()[2].first == Identifier::vanilla("server_links"));
    CHECK(dialogs.protocolIdOf(Identifier::vanilla("custom_options")) == 0);
    CHECK(dialogs.protocolIdOf(Identifier::vanilla("quick_actions")) == 1);
    CHECK(dialogs.protocolIdOf(Identifier::vanilla("server_links")) == 2);
    CHECK(dialogs.ids() == std::vector<Identifier>{Identifier::vanilla("custom_options"),
                                                   Identifier::vanilla("quick_actions"),
                                                   Identifier::vanilla("server_links")});
    CHECK(dialogs.contains(Identifier::vanilla("server_links")));
    CHECK_FALSE(dialogs.contains(Identifier::vanilla("nothing_here")));
    CHECK(dialogs.protocolIdOf(Identifier::vanilla("nothing_here")) == std::nullopt);
}

TEST_CASE("a dialog arrives as the NBT the registry packet carries") {
    FakeJar jar;
    jar.files[entry("server_links")] =
        R"({"type":"minecraft:server_links","button_width":150,"can_close_with_escape":true})";
    const RealDialogs dialogs{jar.handle()};

    REQUIRE(dialogs.size() == 1);
    const NbtCompound& definition = dialogs.entries()[0].second;
    CHECK(definition.getString("type") == "minecraft:server_links");
    CHECK(definition.getInt("button_width") == 150);
    CHECK(definition.getByte("can_close_with_escape") == 1);
    CHECK(definition.size() == 3);
}

TEST_CASE("one dialog that cannot be converted leaves no registry at all") {
    FakeJar jar;
    jar.files[entry("custom_options")] = R"({"type":"minecraft:notice"})";
    std::string reason;

    SUBCASE("malformed JSON") {
        jar.files[entry("server_links")] = "{not json";
        reason = "malformed JSON";
    }
    SUBCASE("a document that is not a compound") {
        jar.files[entry("server_links")] = "[1,2]";
        reason = "not a compound";
    }
    SUBCASE("a list that mixes types") {
        jar.files[entry("server_links")] = R"({"actions":[1,"two"]})";
        reason = "a list mixes types";
    }
    SUBCASE("a null with no NBT equivalent") {
        jar.files[entry("server_links")] = R"({"actions":[null]})";
        reason = "a null has no NBT equivalent";
    }

    CapturedLog log;
    const RealDialogs dialogs{jar.handle()};

    // One warning, naming the file and exactly why it could not travel as NBT.
    REQUIRE(log.lines.size() == 1);
    CHECK(log.lines[0].find("Dialog 'server_links' could not be converted (" + reason + ");") != std::string::npos);

    // The good file sorts first, so a partial registry would have kept it.
    CHECK(dialogs.size() == 0);
    CHECK(dialogs.entries().empty());
    CHECK(dialogs.ids().empty());
    CHECK_FALSE(dialogs.contains(Identifier::vanilla("custom_options")));
}

TEST_CASE("an entry the jar lists but cannot read leaves no registry either") {
    FakeJar jar;
    jar.files[entry("custom_options")] = R"({"type":"minecraft:notice"})";
    RealDialogs::Jar handle = jar.handle();
    handle.readText = [](std::string_view) -> std::optional<std::string> { return std::nullopt; };
    const RealDialogs dialogs{handle};

    CHECK(dialogs.size() == 0);
}

TEST_CASE("a jar that can list but not read is refused before a single entry is read") {
    FakeJar jar;
    jar.files[entry("custom_options")] = R"({"type":"minecraft:notice"})";
    RealDialogs::Jar handle = jar.handle();
    handle.readText = nullptr;
    const RealDialogs dialogs{handle};

    CHECK(dialogs.size() == 0);
}

TEST_CASE("no jar means an empty registry rather than a missing one") {
    const RealDialogs dialogs{RealDialogs::Jar{}};

    CHECK(dialogs.size() == 0);
    CHECK(dialogs.ids().empty());
    CHECK(dialogs.protocolIdOf(Identifier::vanilla("server_links")) == std::nullopt);
}
