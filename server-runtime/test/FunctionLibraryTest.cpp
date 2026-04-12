#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <doctest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace basalt::server::runtime::function;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtTag;

namespace {

[[nodiscard]] std::vector<std::string> linesOf(std::string_view text) {
    std::vector<std::string> kept;
    const auto joined = joinLines(text);
    for (auto& [number, line] : *joined) {
        kept.push_back(line);
    }
    return kept;
}

[[nodiscard]] std::string compileError(std::string_view text) {
    const auto compiled = compileFunction(Identifier{"test", "f"}, text);
    return compiled.has_value() ? std::string{} : compiled.error();
}

} // namespace

TEST_CASE("what a `.mcfunction` keeps, and what it throws away") {
    // parity: Java's trim, which cuts at everything up to and including the space.
    CHECK(linesOf("# a comment\n\n   say hello   \n\t\n   # indented\n\tsay indented\r\nsay goodbye\n") ==
          std::vector<std::string>{"say hello", "say indented", "say goodbye"});
    // A trailing backslash joins the next line, and a comment only counts once the joining is done.
    CHECK(linesOf("say one \\\n   two \\\n   three\nsay after\n") ==
          std::vector<std::string>{"say one two three", "say after"});
    CHECK(linesOf("say \\\n# not a comment\n") == std::vector<std::string>{"say # not a comment"});
    CHECK(linesOf("# still a comment \\\nsay hello\n").empty());
    CHECK_FALSE(joinLines("say one \\").has_value());
    CHECK(compileError("say one \\\n") == "Line continuation at end of file");
}

TEST_CASE("a line compiles as vanilla's fromLines does") {
    CHECK(compileError("/say hi").find("did you mean 'say'? Do not use a preceding forwards slash.") !=
          std::string::npos);
    CHECK(compileError("//comment").find("use '#' not '//'") != std::string::npos);
    CHECK(compileError("$say no variables").find("Can't parse function line 1") != std::string::npos);
    CHECK(compileError("$say $(open").find("Can't parse function line 1") != std::string::npos);
    CHECK(compileError("$say $(bad-name)").find("Can't parse function line 1") != std::string::npos);

    const auto checked = compileFunction(Identifier{"test", "f"}, "say ok\nbroken\n", [](const std::string& line) {
        return line == "broken" ? std::optional<std::string>{"Unknown command"} : std::nullopt;
    });
    REQUIRE_FALSE(checked.has_value());
    CHECK(checked.error() == "Whilst parsing command on line 2: Unknown command");
}

TEST_CASE("a macro indexes its variables in first-seen order and fills them in") {
    const auto function = compileFunction(Identifier{"test", "m"}, "say plain\n$say $(b) and $(a) then $(b)$\n");
    REQUIRE(function.has_value());
    CHECK(function->isMacro);
    CHECK(function->parameters == std::vector<std::string>{"b", "a"});
    REQUIRE(function->entries[1].macro.has_value());
    CHECK(function->entries[1].macro->substitute({"x", "y"}) == "say x and y then x$");

    // parity: `MacroFunction.stringify`: numbers without an exponent or a trailing zero.
    CHECK(macroArgument(NbtTag::ofDouble(1.0)) == "1");
    CHECK(macroArgument(NbtTag::ofDouble(0.1)) == "0.1");
    CHECK(macroArgument(NbtTag::ofDouble(1e20)) == "100000000000000000000");
    CHECK(macroArgument(NbtTag::ofFloat(2.5F)) == "2.5");
    CHECK(macroArgument(NbtTag::ofByte(3)) == "3");
    CHECK(macroArgument(NbtTag::ofLong(-4)) == "-4");
    CHECK(macroArgument(NbtTag::ofString("text")) == "text");
}

TEST_CASE("the library reads every namespace and keeps a tag's order without repeats") {
    auto files = std::make_shared<std::map<std::string, std::string>>(std::map<std::string, std::string>{
        {"data/a/function/one.mcfunction", "say one\n"},
        {"data/b/function/deep/two.mcfunction", "say two\n"},
        {"data/b/function/broken.mcfunction", "/say broken\n"},
        {"data/minecraft/tags/function/tick.json", R"({"values":["b:deep/two","a:one","b:deep/two"]})"},
        {"data/minecraft/tags/function/load.json", R"({"values":["b:broken"]})"},
    });
    FunctionLibrary library;
    const std::size_t loaded = library.load(
        [files](std::string_view prefix) {
            std::vector<std::string> names;
            for (const auto& [name, text] : *files) {
                if (name.starts_with(prefix)) {
                    names.push_back(name);
                }
            }
            return names;
        },
        [files](std::string_view name) -> std::optional<std::string> {
            const auto found = files->find(std::string{name});
            return found == files->end() ? std::nullopt : std::optional{found->second};
        });
    CHECK(loaded == 2);
    CHECK(library.get(Identifier{"b", "deep/two"}) != nullptr);
    CHECK(library.get(Identifier{"b", "broken"}) == nullptr);
    const std::vector<const ServerFunction*> tick = library.tag(Identifier::vanilla("tick"));
    REQUIRE(tick.size() == 2);
    CHECK(tick[0]->id == Identifier{"b", "deep/two"});
    CHECK(tick[1]->id == Identifier{"a", "one"});
    // A tag naming a function that did not load is not loaded either.
    CHECK_FALSE(library.hasTag(Identifier::vanilla("load")));
}
