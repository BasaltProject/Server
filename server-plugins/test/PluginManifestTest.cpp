#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginManifest.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::server::plugins;

namespace {

/// The oracle prints Kotlin collections, which are `[a, b]` and `[]` when empty.
[[nodiscard]] std::string bracketed(const std::vector<std::string>& values) {
    std::string text = "[";
    for (const std::string& value : values) {
        if (text.size() > 1) {
            text += ", ";
        }
        text += value;
    }
    return text + "]";
}

[[nodiscard]] std::string describe(const PluginManifest& manifest) {
    std::vector<std::string> dependencies;
    for (const PluginDependency& dependency : manifest.dependencies) {
        dependencies.push_back(dependency.id + dependency.requirement + (dependency.hard ? "!" : "?"));
    }
    std::vector<std::string> hard;
    for (const PluginDependency& dependency : manifest.hardDependencies()) {
        hard.push_back(dependency.id);
    }
    std::vector<std::string> asked;
    for (const PluginPermission permission : manifest.capabilities.asks()) {
        asked.emplace_back(permissionName(permission));
    }
    std::sort(asked.begin(), asked.end());

    const auto flag = [](bool value) { return value ? "true" : "false"; };
    return manifest.id + "/" + manifest.name + "/" + manifest.version + "/" + std::to_string(manifest.api) + "/" +
           manifest.main + "/authors=" + bracketed(manifest.authors) + "/deps=" + bracketed(dependencies) +
           "/hard=" + bracketed(hard) + "/caps=" + flag(manifest.capabilities.storage) + "," +
           flag(manifest.capabilities.playersData) + "," + flag(manifest.capabilities.commands) + "," +
           flag(manifest.capabilities.packets) + "," + bracketed(manifest.capabilities.files) + "," +
           bracketed(manifest.capabilities.network) + "/asks=" + bracketed(asked) +
           "/exports=" + bracketed(manifest.exports.modules) + "," + bracketed(manifest.exports.services) + "," +
           flag(manifest.exports.opensEverything()) + "/private=" + bracketed(manifest.privateResources);
}

[[nodiscard]] std::string refusal(std::string_view text) {
    const std::expected<PluginManifest, ManifestError> manifest = PluginManifest::parse(text);
    if (manifest.has_value()) {
        return "accepted";
    }
    std::string message = manifest.error().message;
    for (char& character : message) {
        if (character == ' ') {
            character = '_';
        }
    }
    return message;
}

// Everything a manifest can say, in one file.
constexpr std::string_view kFull = R"(id = "arena"
name = "Arena"
version = "2.1.0"
authors = ["Whiron", "Ana"]
api = 1
main = "src/arena.lua"

[depends]
economy = ">=1.4"
worldedit = "*"
# A requirement that is not a string at all, which means any version rather than none.
permissions = 3

[soft-depends]
chat = "<2.0"

[capabilities]
storage = true
players_data = true
commands = true
packets = true
files = ["maps/", "schematics/"]
network = ["https://arena.example/"]

[exports]
modules = ["api", "util"]
services = ["arena"]

[resources]
private = ["templates/", "seed.json"])";

} // namespace

TEST_CASE("a manifest reads the way the frozen kotlin reads it") {
    const std::expected<PluginManifest, ManifestError> full = PluginManifest::parse(kFull);
    REQUIRE(full.has_value());
    CHECK(
        describe(*full) ==
        "arena/Arena/2.1.0/1/src/arena.lua/authors=[Whiron, Ana]"
        "/deps=[economy>=1.4!, worldedit*!, permissions*!, chat<2.0?]"
        "/hard=[economy, worldedit, permissions]/caps=true,true,true,true,[maps/, schematics/],[https://arena.example/]"
        "/asks=[commands, network, packets]/exports=[api, util],[arena],false/private=[templates/, seed.json]");

    // The smallest file that loads: everything else takes its default.
    const std::expected<PluginManifest, ManifestError> minimal =
        PluginManifest::parse("id = \"hello\"\nversion = \"1.0.0\"\napi = 1");
    REQUIRE(minimal.has_value());
    CHECK(describe(*minimal) ==
          "hello/hello/1.0.0/1/src/main.lua/authors=[]/deps=[]/hard=[]/caps=false,false,false,false,[],[]"
          "/asks=[]/exports=[],[],false/private=[]");

    // A module list that opens everything.
    const std::expected<PluginManifest, ManifestError> open =
        PluginManifest::parse("id = \"toolkit\"\nversion = \"0.1\"\napi = 1\n\n[exports]\nmodules = [\"*\"]");
    REQUIRE(open.has_value());
    CHECK(describe(*open) ==
          "toolkit/toolkit/0.1/1/src/main.lua/authors=[]/deps=[]/hard=[]/caps=false,false,false,false,[],[]"
          "/asks=[]/exports=[*],[],true/private=[]");
}

TEST_CASE("a manifest the loader cannot work with is refused, and says why") {
    CHECK(refusal("version = \"1\"\napi = 1") == "plugin.toml_has_no_'id'");
    CHECK(refusal("id = \"Arena\"\nversion = \"1\"\napi = 1") ==
          "'Arena'_is_not_a_usable_id:_lowercase_letters,_digits_and_underscores,_starting_with_a_letter");
    CHECK(refusal("id = \"1arena\"\nversion = \"1\"\napi = 1") ==
          "'1arena'_is_not_a_usable_id:_lowercase_letters,_digits_and_underscores,_starting_with_a_letter");
    CHECK(refusal("id = \"my-plugin\"\nversion = \"1\"\napi = 1") ==
          "'my-plugin'_is_not_a_usable_id:_lowercase_letters,_digits_and_underscores,_starting_with_a_letter");
    CHECK(refusal("id = \"arena\"\napi = 1") == "arena_has_no_'version'");
    CHECK(refusal("id = \"arena\"\nversion = \"1\"") == "arena_does_not_say_which_'api'_it_was_written_for");
    // The reader's own reason is repeated here, which is what tells the author where to look.
    CHECK(refusal("id = \nversion").starts_with("plugin.toml_is_not_valid_TOML:"));
}

TEST_CASE("versions compare the way the frozen kotlin compares them") {
    CHECK(compareVersions("1.0.0", "1.0.0") == 0);
    // A missing part is zero, so a shorter version is not automatically older.
    CHECK(compareVersions("1.0", "1.0.0") == 0);
    CHECK(compareVersions("1.2.3", "1.2.4") == -1);
    // Numeric, not lexicographic: ten is after nine.
    CHECK(compareVersions("1.10.0", "1.9.0") == 1);
    CHECK(compareVersions("2.0", "1.99.99") == 1);
    // A part that is not a number counts as zero, which is why a beta compares equal to the release.
    CHECK(compareVersions("1.0.0-beta", "1.0.0") == 0);
    CHECK(compareVersions("1.0.0-2", "1.0.0-10") == -1);
    CHECK(compareVersions("", "0") == 0);
    CHECK(compareVersions("1.0.0", "1.0.0.0") == 0);
}

TEST_CASE("a requirement accepts what the frozen kotlin accepts") {
    static constexpr std::string_view kCandidates[] = {"1.2.3", "1.2.0", "2.0.0", "0.9"};
    const auto accepts = [](std::string_view requirement) {
        std::string answers;
        for (const std::string_view candidate : kCandidates) {
            if (!answers.empty()) {
                answers += ',';
            }
            answers +=
                PluginDependency{.id = "x", .requirement = std::string{requirement}, .hard = true}.accepts(candidate)
                    ? "true"
                    : "false";
        }
        return answers;
    };

    CHECK(accepts("*") == "true,true,true,true");
    CHECK(accepts("1.2.3") == "true,false,false,false");
    CHECK(accepts(">=1.2") == "true,true,true,false");
    // The space after the operator is trimmed, so a hand-written requirement still works.
    CHECK(accepts("> 1.2") == "true,false,true,false");
    CHECK(accepts("<=2.0") == "true,true,true,true");
    CHECK(accepts("<2.0") == "true,true,false,true");
    CHECK(accepts("  1.2.3  ") == "true,false,false,false");
}

TEST_CASE("what a plugin exports and what an operator has to grant") {
    const PluginExports exports{.modules = {"api", "util"}, .services = {}};
    CHECK(exports.exportsModule("api"));
    CHECK_FALSE(exports.exportsModule("secret"));
    CHECK(PluginExports{.modules = {"*"}, .services = {}}.exportsModule("anything"));
    CHECK_FALSE(PluginExports{}.exportsModule("api"));

    CHECK(permissionName(PluginPermission::Packets) == "packets");
    CHECK(permissionName(PluginPermission::Network) == "network");
    CHECK(permissionName(PluginPermission::Commands) == "commands");
    CHECK(permissionOf("packets") == PluginPermission::Packets);
    CHECK_FALSE(permissionOf("nope").has_value());

    // Network is asked for by naming a host, not by a flag, so an empty list asks for nothing.
    CHECK(PluginCapabilities{.network = {"https://x/"}}.asks() ==
          std::set<PluginPermission>{PluginPermission::Network});
    CHECK(PluginCapabilities{.network = {}}.asks().empty());

    CHECK(PluginManifest::kApi == 1);
    CHECK(PluginManifest::kFileName == "plugin.toml");
}
