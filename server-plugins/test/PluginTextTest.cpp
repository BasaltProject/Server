#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginText.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace basalt::server::plugins;
namespace fileIO = basalt::wire::io::fileIO;

namespace {
[[nodiscard]] std::string scratch() {
    return (std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / "oracle_text").string();
}

void write(const std::string& path, std::string_view text) {
    fileIO::createDirectories(path.substr(0, path.find_last_of('/')));
    fileIO::writeText(path, text);
}

const PluginText::Fields kFields = [](std::string_view name) -> std::optional<std::string> {
    if (name == "name") {
        return "Whiron";
    }
    if (name == "player") {
        return "Ana";
    }
    if (name == "arena") {
        return "Sky";
    }
    if (name == "count") {
        return "3";
    }
    return std::nullopt;
};
} // namespace

TEST_CASE("a plugin's own text resolves with its fields") {
    const std::string folder = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(folder, ignored);
    write(folder + "/lang/en_us.toml", R"(greeting = "Hello, {name}!"
plain = "Nothing to fill"

[arena]
joined = "{player} joined {arena}")");
    write(folder + "/lang/es_es.toml", R"(greeting = "Hola, {name}!"
solo_espanol = "Solo aqui")");
    write(folder + "/lang/notes.txt", "ignored");
    write(folder + "/lang/broken.toml", "this = ");

    PluginText text{folder, "en_us"};
    text.load();
    CHECK(text.localeCount() == 2);

    const auto resolve = [&](std::string_view message) { return text.resolve(message, kFields); };

    CHECK(resolve("@greeting") == "Hello, Whiron!");
    CHECK(resolve("@arena.joined") == "Ana joined Sky");
    CHECK(resolve("@plain") == "Nothing to fill");
    CHECK(resolve("@nope.at.all") == "nope.at.all");
    CHECK(resolve("@solo_espanol") == "Solo aqui");
    CHECK(resolve("Plain text") == "Plain text");
    CHECK(resolve("Hello, {name}!") == "Hello, Whiron!");
    CHECK(resolve("Hello, {nobody}!") == "Hello, {nobody}!");
    CHECK(resolve("{player} in {arena} with {count}") == "Ana in Sky with 3");
    CHECK(resolve("Hello, {name") == "Hello, {name");
    CHECK(resolve("").empty());
    CHECK(resolve("{}") == "{}");

    CHECK(text.resolve("Hello, {name}!") == "Hello, {name}!");

    PluginText spanish{folder, "es_es"};
    spanish.load();
    CHECK(spanish.resolve("@greeting", kFields) == "Hola, Whiron!");
    CHECK(spanish.resolve("@plain", kFields) == "Nothing to fill");

    std::filesystem::remove_all(folder, ignored);
}
