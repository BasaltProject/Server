#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginFolder.hpp>
#include <basalt/server/plugins/PluginPackage.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::plugins;
namespace fileIO = basalt::wire::io::fileIO;

namespace {

[[nodiscard]] std::string scratch() {
    const std::filesystem::path directory{BASALT_TEST_SCRATCH_DIR};
    return (directory / "oracle_plugins").string();
}

void write(const std::string& path, std::string_view text) {
    fileIO::createDirectories(path.substr(0, path.find_last_of('/')));
    fileIO::writeText(path, text);
}

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

[[nodiscard]] std::string without(std::string_view path, std::string_view prefix) {
    return std::string{path.starts_with(prefix) ? path.substr(prefix.size()) : path};
}

constexpr std::string_view kManifestText = R"(id = "arena"
version = "1.0.0"
api = 1

[resources]
# A pattern with a leading slash, which is the same pattern.
private = ["/templates/", "seed.json"])";

/// The same files, saying it is a built package: what a `.bspkg` looks like to the copier.
class NotEditable final : public PluginPackage {
public:
    explicit NotEditable(const PluginPackage& inner) : PluginPackage(inner.origin()), inner_(&inner) {}

    [[nodiscard]] bool editable() const noexcept override { return false; }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path) const override {
        return inner_->read(path);
    }

    [[nodiscard]] std::vector<std::string> list(std::string_view prefix) const override { return inner_->list(prefix); }

private:
    const PluginPackage* inner_;
};

/// A stored (uncompressed) zip, built here so nothing of anybody's ships in the repository. Only the
/// archive half of the tests uses this: the frozen Kotlin has no zip writer to compare against, so
/// what it pins is that a package and a folder answer the same questions the same way.
[[nodiscard]] std::vector<std::uint8_t> storedZip(const std::vector<std::pair<std::string, std::string>>& entries) {
    std::vector<std::uint8_t> out;
    const auto put16 = [&out](std::uint32_t value) {
        out.push_back(static_cast<std::uint8_t>(value & 0xFF));
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    };
    const auto put32 = [&out](std::uint32_t value) {
        for (std::int32_t shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
        }
    };
    const auto crc32 = [](std::string_view data) {
        std::uint32_t value = 0xFFFFFFFFU;
        for (const char character : data) {
            value ^= static_cast<std::uint8_t>(character);
            for (std::int32_t bit = 0; bit < 8; ++bit) {
                value = (value >> 1) ^ (0xEDB88320U & (~(value & 1U) + 1U));
            }
        }
        return value ^ 0xFFFFFFFFU;
    };

    std::vector<std::uint32_t> offsets;
    for (const auto& [name, body] : entries) {
        offsets.push_back(static_cast<std::uint32_t>(out.size()));
        put32(0x04034B50);
        put16(20);
        put16(0);
        put16(0);
        put16(0);
        put16(0);
        put32(crc32(body));
        put32(static_cast<std::uint32_t>(body.size()));
        put32(static_cast<std::uint32_t>(body.size()));
        put16(static_cast<std::uint32_t>(name.size()));
        put16(0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), body.begin(), body.end());
    }
    const std::uint32_t directoryStart = static_cast<std::uint32_t>(out.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& [name, body] = entries[index];
        put32(0x02014B50);
        put16(20);
        put16(20);
        put16(0);
        put16(0);
        put16(0);
        put16(0);
        put32(crc32(body));
        put32(static_cast<std::uint32_t>(body.size()));
        put32(static_cast<std::uint32_t>(body.size()));
        put16(static_cast<std::uint32_t>(name.size()));
        put16(0);
        put16(0);
        put16(0);
        put16(0);
        put32(0);
        put32(offsets[index]);
        out.insert(out.end(), name.begin(), name.end());
    }
    const std::uint32_t directorySize = static_cast<std::uint32_t>(out.size()) - directoryStart;
    put32(0x06054B50);
    put16(0);
    put16(0);
    put16(static_cast<std::uint32_t>(entries.size()));
    put16(static_cast<std::uint32_t>(entries.size()));
    put32(directorySize);
    put32(directoryStart);
    put16(0);
    return out;
}

} // namespace

TEST_CASE("a folder plugin answers what the frozen kotlin answers") {
    const std::string root = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    // A folder plugin, with resources of every shape the copier has to keep.
    write(root + "/arena/plugin.toml", kManifestText);
    write(root + "/arena/src/main.lua", "return {}");
    write(root + "/arena/src/util.lua", "return {}");
    write(root + "/arena/resources/config.toml", "balance = 100");
    write(root + "/arena/resources/lang/en_us.json", "{}");
    write(root + "/arena/resources/templates/map.json", "{}");
    write(root + "/arena/resources/seed.json", "{}");
    // A name the private 'seed.json' must not swallow: it is a file name, not a prefix.
    write(root + "/arena/resources/seed.json.bak", "{}");
    // A directory with no manifest is not a plugin, and neither is a loose file.
    write(root + "/notes/readme.txt", "hello");
    write(root + "/stray.txt", "hello");

    const std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError> packages = discoverPackages(root);
    REQUIRE(packages.has_value());
    REQUIRE(packages->size() == 1);
    const PluginPackage& folder = *packages->front();
    CHECK(without(folder.origin(), root + "/") == "arena");
    CHECK(folder.editable());
    CHECK(without(folder.workingFolder().value(), root + "/") == "arena/resources");

    CHECK(bracketed(folder.list("")) ==
          "[plugin.toml, resources/config.toml, resources/lang/en_us.json, resources/seed.json, "
          "resources/seed.json.bak, resources/templates/map.json, src/main.lua, src/util.lua]");
    CHECK(bracketed(folder.list(packageLayout::kSource)) == "[src/main.lua, src/util.lua]");
    CHECK(bracketed(folder.list(packageLayout::kResources)) ==
          "[resources/config.toml, resources/lang/en_us.json, resources/seed.json, resources/seed.json.bak, "
          "resources/templates/map.json]");
    // With or without the trailing slash is the same question.
    CHECK(bracketed(folder.list("src")) == "[src/main.lua, src/util.lua]");
    CHECK(folder.readText("resources/config.toml") == "balance = 100");
    CHECK_FALSE(folder.readText("nope").has_value());
    CHECK(folder.exists("src/main.lua"));
    CHECK_FALSE(folder.exists("src/nope.lua"));

    std::filesystem::remove_all(root, ignored);
}

TEST_CASE("what a package puts in a plugin's folder, and what it leaves alone") {
    const std::string root = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    write(root + "/arena/plugin.toml", kManifestText);
    write(root + "/arena/resources/config.toml", "balance = 100");
    write(root + "/arena/resources/lang/en_us.json", "{}");
    write(root + "/arena/resources/templates/map.json", "{}");
    write(root + "/arena/resources/seed.json", "{}");
    write(root + "/arena/resources/seed.json.bak", "{}");

    const FolderPackage folder{root + "/arena"};
    const std::expected<PluginManifest, ManifestError> manifest = PluginManifest::parse(kManifestText);
    REQUIRE(manifest.has_value());

    // A folder plugin is its own working folder, so nothing is copied next to it.
    std::vector<std::string> written;
    PluginFolder{folder, *manifest, root + "/arena-out"}.materialize(
        [&](std::string_view line) { written.emplace_back(line); });
    CHECK(written.empty());

    // The live folder of a built package, which is what the copier really does.
    const std::string live = root + "/live";
    write(live + "/config.toml", "balance = 999");
    const NotEditable packed{folder};
    std::vector<std::string> copied;
    PluginFolder{packed, *manifest, live}.materialize(
        [&](std::string_view line) { copied.push_back(without(line, "wrote " + live + "/")); });
    std::sort(copied.begin(), copied.end());
    // Only the one that was missing and not private: `config.toml` was already there, and the
    // templates and the seed stay inside the package.
    CHECK(bracketed(copied) == "[lang/en_us.json, seed.json.bak]");
    // The file that was already there is untouched: an update never overwrites what was edited.
    CHECK(fileIO::readText(live + "/config.toml") == "balance = 999");

    const std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> index =
        PluginFolder{folder, *manifest, live}.index();
    REQUIRE(index.has_value());
    std::vector<std::string> keys;
    for (const auto& [key, path] : *index) {
        keys.push_back(key);
    }
    CHECK(bracketed(keys) == "[config.toml, lang/en_us.json, seed.json.bak]");

    // Two files addressed by the same name, at different depths.
    write(live + "/nested/config.toml", "clash = true");
    const std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> clash =
        PluginFolder{folder, *manifest, live}.index();
    REQUIRE_FALSE(clash.has_value());
    CHECK(clash.error().message == "two files are called 'config.toml' (" + live + "/config.toml and " + live +
                                       "/nested/config.toml), and one name has to mean one file");

    // The same rule for json, which is addressed by name too.
    fileIO::remove(live + "/nested/config.toml");
    write(live + "/nested/en_us.json", "{}");
    const std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> jsonClash =
        PluginFolder{folder, *manifest, live}.index();
    REQUIRE_FALSE(jsonClash.has_value());
    CHECK(jsonClash.error().message == "two files are called 'en_us.json' (" + live + "/lang/en_us.json and " + live +
                                           "/nested/en_us.json), and one name has to mean one file");

    std::filesystem::remove_all(root, ignored);
}

TEST_CASE("an empty plugins directory is made rather than refused") {
    const std::string root = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const std::string fresh = root + "/fresh";
    const std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError> packages =
        discoverPackages(fresh);
    REQUIRE(packages.has_value());
    CHECK(packages->empty());
    CHECK(fileIO::isDirectory(fresh));

    std::filesystem::remove_all(root, ignored);
}

TEST_CASE("a built package answers the same questions as a folder") {
    const std::string root = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    fileIO::createDirectories(root);

    const std::vector<std::uint8_t> zip = storedZip({{"plugin.toml", std::string{kManifestText}},
                                                     // A directory entry, which is not a file.
                                                     {"src/", ""},
                                                     {"src/main.lua", "return {}"},
                                                     {"src/util.lua", "return {}"},
                                                     {"resources/config.toml", "balance = 100"},
                                                     {"resources/lang/en_us.json", "{}"}});
    fileIO::writeBytes(root + "/zebra.bspkg", zip);
    // Written second but sorted first: what is found is in name order, not the order the filesystem
    // happens to hand back.
    fileIO::writeBytes(root + "/arena.bspkg", zip);
    // A file with the wrong extension is not a package, whatever is inside it.
    fileIO::writeBytes(root + "/arena.zip", zip);

    const std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError> packages = discoverPackages(root);
    REQUIRE(packages.has_value());
    REQUIRE(packages->size() == 2);
    CHECK(without((*packages)[0]->origin(), root + "/") == "arena.bspkg");
    CHECK(without((*packages)[1]->origin(), root + "/") == "zebra.bspkg");
    const PluginPackage& archive = *packages->front();
    CHECK_FALSE(archive.editable());
    CHECK_FALSE(archive.workingFolder().has_value());
    CHECK(bracketed(archive.list("")) ==
          "[plugin.toml, resources/config.toml, resources/lang/en_us.json, src/main.lua, src/util.lua]");
    CHECK(bracketed(archive.list(packageLayout::kSource)) == "[src/main.lua, src/util.lua]");
    CHECK(archive.readText("resources/config.toml") == "balance = 100");
    CHECK_FALSE(archive.readText("nope").has_value());
    CHECK(archive.exists("plugin.toml"));

    // A file that is not a readable archive is named, not skipped in silence.
    fileIO::writeText(root + "/broken.bspkg", "not a zip");
    const std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError> broken = discoverPackages(root);
    REQUIRE_FALSE(broken.has_value());
    CHECK(broken.error().message.find("broken.bspkg is damaged") != std::string::npos);

    std::filesystem::remove_all(root, ignored);
}

TEST_CASE("a module is named the way lua names one") {
    const auto file = [](std::string_view module) { return packageLayout::moduleFile(module).value_or("<no>"); };

    CHECK(file("coins") == "src/coins.lua");
    CHECK(file("util.money") == "src/util/money.lua");
    CHECK(file("a.b.c") == "src/a/b/c.lua");

    // Writing the extension is not a second way to say `coins`: the dot separates, so it asks for
    // a file that is not there rather than for that one under another name.
    CHECK(file("coins.lua") == "src/coins/lua.lua");

    // Neither is a path.
    CHECK(file("util/money") == "<no>");
    CHECK(file("") == "<no>");
    CHECK(file(".coins") == "<no>");
    CHECK(file("coins.") == "<no>");
    CHECK(file("util..money") == "<no>");
    // `..` cannot climb out of `src/`, whichever separator it is written with.
    CHECK(file("...") == "<no>");
    CHECK(file("..\\..\\etc") == "<no>");
}
