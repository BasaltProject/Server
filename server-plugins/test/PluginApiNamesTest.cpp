#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>
#include "../src/PluginFilePath.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
[[nodiscard]] std::string slurp(const std::filesystem::path& path) {
    std::ifstream in{path};
    std::stringstream out;
    out << in.rdbuf();
    return out.str();
}
} // namespace

TEST_CASE("every function the Lua API installs is listed in the Kiln API manifest mirrored here") {
    const std::filesystem::path root{BASALT_PLUGINS_DIR};
    std::set<std::string> listed;
    std::istringstream manifest{slurp(root / "test" / "kiln-server.api")};
    for (std::string line; std::getline(manifest, line);) {
        if (!line.empty() && line.front() != '#' && line.front() != '@') {
            listed.insert(line.substr(0, line.find(' ')));
        }
    }
    REQUIRE_FALSE(listed.empty());

    static const std::set<std::string> kInternal{"cancel", "data.index", "data.newindex", "store.top",
                                                 "store.index", "store.newindex"};
    const std::regex call{R"re((?:pushHostFunction|installWaitingFunction)\(\s*[\w*.>-]+,\s*(\w+ \? )?("[\w.]+")( : ("[\w.]+"))?)re"};
    std::set<std::string> missing;
    for (const auto& entry : std::filesystem::directory_iterator{root / "src"}) {
        const std::string source = slurp(entry.path());
        for (std::sregex_iterator it{source.begin(), source.end(), call}, end; it != end; ++it) {
            for (const std::size_t group : {2U, 4U}) {
                std::string name = (*it)[group].str();
                if (name.size() > 2) {
                    name = name.substr(1, name.size() - 2);
                    if (!kInternal.contains(name) && !listed.contains(name)) {
                        missing.insert(name);
                    }
                }
            }
        }
    }
    std::string report;
    for (const std::string& name : missing) {
        report += name + " ";
    }
    CHECK_MESSAGE(missing.empty(), "add to Kiln api/server.api and DevTools, then mirror it here: " << report);
}

TEST_CASE("Lua file paths reject symlink escapes and retain plugin-local access") {
    namespace fs = std::filesystem;
    using basalt::server::plugins::pluginApi::pluginFilePath;
    const fs::path scratch = fs::temp_directory_path() / "basalt-plugin-path-test";
    fs::remove_all(scratch);
    fs::create_directories(scratch / "plugin" / "local");
    fs::create_directories(scratch / "outside");
    fs::create_directory_symlink(scratch / "outside", scratch / "plugin" / "escape");
    fs::create_directory_symlink(scratch / "plugin" / "local", scratch / "plugin" / "inside");
    CHECK(pluginFilePath((scratch / "plugin").string(), "local/new/data.txt").has_value());
    CHECK(pluginFilePath((scratch / "plugin").string(), "inside/file.txt") == fs::weakly_canonical(scratch / "plugin/local/file.txt").string());
    for (const std::string& path : std::vector<std::string>{"../outside/file", "/etc/passwd", "escape/file", "escape/new/file", "C:/file", "local\\file", std::string("a\0b",3)}) {
        CHECK_FALSE(pluginFilePath((scratch / "plugin").string(), path).has_value());
    }
    fs::remove_all(scratch);
}
