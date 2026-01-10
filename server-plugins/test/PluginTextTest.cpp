#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginText.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::plugins;
using namespace basalt::server::lua;
namespace fileIO = basalt::wire::io::fileIO;

namespace {

[[nodiscard]] std::string scratch() {
    return (std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / "oracle_text").string();
}

void write(const std::string& path, std::string_view text) {
    fileIO::createDirectories(path.substr(0, path.find_last_of('/')));
    fileIO::writeText(path, text);
}

/// A sandbox with the arguments table sitting at stack index one, which is where a host function
/// would find it: the resolver is exercised against a real Lua table rather than a stand-in.
class Arguments {
public:
    Arguments() {
        sandbox_ = LuaSandbox::create("arena");
        lua_ = std::make_unique<LuaArgs>(sandbox_->state());
        lua_->pushTable();
        lua_->pushString("Whiron");
        lua_->setField("name");
        lua_->pushString("Ana");
        lua_->setField("player");
        lua_->pushString("Sky");
        lua_->setField("arena");
        lua_->pushInteger(3);
        lua_->setField("count");
        lua_->pushBoolean(true);
        lua_->setField("flag");
    }

    [[nodiscard]] LuaArgs& lua() { return *lua_; }

    /// Pushes something that is not a table, for the case where a plugin passed the wrong thing.
    [[nodiscard]] std::int32_t notATable() {
        lua_->pushString("nope");
        return lua_->count();
    }

private:
    std::unique_ptr<LuaSandbox> sandbox_;
    std::unique_ptr<LuaArgs> lua_;
};

} // namespace

TEST_CASE("a plugin's own text resolves the way the frozen kotlin resolves it") {
    const std::string folder = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(folder, ignored);
    write(folder + "/lang/en_us.toml", R"(greeting = "Hello, {name}!"
plain = "Nothing to fill"

[arena]
joined = "{player} joined {arena}")");
    write(folder + "/lang/es_es.toml", R"(greeting = "Hola, {name}!"
solo_espanol = "Solo aqui")");
    // Not a locale file, and one that is not readable TOML: both skipped rather than fatal.
    write(folder + "/lang/notes.txt", "ignored");
    write(folder + "/lang/broken.toml", "this = ");

    PluginText text{folder, "en_us"};
    text.load();
    CHECK(text.localeCount() == 2);

    Arguments arguments;
    const auto resolve = [&](std::string_view message) { return text.resolve(message, &arguments.lua(), 1); };

    CHECK(resolve("@greeting") == "Hello, Whiron!");
    CHECK(resolve("@arena.joined") == "Ana joined Sky");
    CHECK(resolve("@plain") == "Nothing to fill");
    // A key nothing translates comes back as the key.
    CHECK(resolve("@nope.at.all") == "nope.at.all");
    // Only Spanish has this one, and the default locale does not: any locale will do.
    CHECK(resolve("@solo_espanol") == "Solo aqui");
    CHECK(resolve("Plain text") == "Plain text");
    CHECK(resolve("Hello, {name}!") == "Hello, Whiron!");
    // An argument that was not passed stays as it was written.
    CHECK(resolve("Hello, {nobody}!") == "Hello, {nobody}!");
    CHECK(resolve("{player} in {arena} with {count} and {flag}") == "Ana in Sky with 3 and true");
    // A brace that never closes takes the rest of the line with it.
    CHECK(resolve("Hello, {name") == "Hello, {name");
    CHECK(resolve("").empty());
    CHECK(resolve("{}") == "{}");

    // No table at all, and something that is not one: both leave the braces alone.
    CHECK(text.resolve("Hello, {name}!", nullptr, 0) == "Hello, {name}!");
    const std::int32_t wrong = arguments.notATable();
    CHECK(text.resolve("Hello, {name}!", &arguments.lua(), wrong) == "Hello, {name}!");
    // A string is not a table, but Lua will happily index one through its metatable: `{upper}` on a
    // string argument would come back as the standard library's own function if nobody checked.
    CHECK(text.resolve("Hello, {upper}!", &arguments.lua(), wrong) == "Hello, {upper}!");

    // The same files with Spanish as the default: which locale is preferred has to be the one asked
    // for, not whichever sorts first.
    PluginText spanish{folder, "es_es"};
    spanish.load();
    CHECK(spanish.resolve("@greeting", &arguments.lua(), 1) == "Hola, Whiron!");
    // What Spanish does not have still comes from the other one.
    CHECK(spanish.resolve("@plain", &arguments.lua(), 1) == "Nothing to fill");

    // Every lookup has to leave the stack as it found it: a host function called once per message
    // would otherwise run the interpreter out of stack in a few thousand lines of chat.
    const std::int32_t before = arguments.lua().count();
    for (std::int32_t round = 0; round < 200; ++round) {
        static_cast<void>(text.resolve("{name} {player} {arena} {count} {flag} {nobody}", &arguments.lua(), 1));
    }
    CHECK(arguments.lua().count() == before);

    std::filesystem::remove_all(folder, ignored);
}

TEST_CASE("a plugin with no lang folder still says something") {
    const std::string folder = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(folder, ignored);

    PluginText bare{folder + "/nothing", "en_us"};
    bare.load();
    CHECK(bare.localeCount() == 0);
    Arguments arguments;
    // The key itself rather than an empty line: a missing string has to look missing.
    CHECK(bare.resolve("@greeting", &arguments.lua(), 1) == "greeting");
    CHECK(bare.resolve("Hi", &arguments.lua(), 1) == "Hi");
    CHECK_FALSE(bare.translation("greeting").has_value());

    std::filesystem::remove_all(folder, ignored);
}
