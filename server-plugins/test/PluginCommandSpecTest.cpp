#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginCommandSpec.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::server::plugins;

namespace {

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

[[nodiscard]] std::string describe(std::string_view spec) {
    const std::expected<PluginCommandSpec, ManifestError> parsed = PluginCommandSpec::parse(spec, "basalt.arena");
    if (!parsed.has_value()) {
        return "refused";
    }
    std::vector<std::string> arguments;
    for (const PluginCommandArgument& argument : parsed->arguments) {
        arguments.push_back(argument.name + "/" + std::string{spellingOf(argument.type)} + "/" +
                            (argument.optional ? "true" : "false") + "/" +
                            (argument.min.has_value() ? std::to_string(*argument.min) : "null") + "/" +
                            (argument.max.has_value() ? std::to_string(*argument.max) : "null"));
    }
    return "name=" + parsed->name() + " literals=" + bracketed(parsed->literals) + " args=" + bracketed(arguments) +
           " usage=" + parsed->usage() + " permission=" + parsed->permission.value_or("null");
}

[[nodiscard]] std::string refusal(std::string_view spec) {
    const std::expected<PluginCommandSpec, ManifestError> parsed = PluginCommandSpec::parse(spec);
    if (parsed.has_value()) {
        return "accepted";
    }
    std::string message = parsed.error().message;
    for (char& character : message) {
        if (character == ' ') {
            character = '_';
        }
    }
    return message;
}

} // namespace

TEST_CASE("a command spec reads the way the frozen kotlin reads it") {
    CHECK(describe("arena") == "name=arena literals=[arena] args=[] usage=arena permission=basalt.arena");
    CHECK(describe("arena create <name: word>") ==
          "name=arena literals=[arena, create] args=[name/word/false/null/null] "
          "usage=arena create <name: word> permission=basalt.arena");
    CHECK(describe("arena create <name: word> [size: int 8..64]") ==
          "name=arena literals=[arena, create] args=[name/word/false/null/null, size/int/true/8/64] "
          "usage=arena create <name: word> [size: int 8..64] permission=basalt.arena");

    // Several literals and every type, in one line.
    CHECK(describe("shop admin set <player: word> <amount: int> <rate: float> <silent: bool> <reason: text>") ==
          "name=shop literals=[shop, admin, set] args=[player/word/false/null/null, amount/int/false/null/null, "
          "rate/float/false/null/null, silent/bool/false/null/null, reason/text/false/null/null] "
          "usage=shop admin set <player: word> <amount: int> <rate: float> <silent: bool> <reason: text> "
          "permission=basalt.arena");

    // No type at all, which is a word.
    CHECK(describe("kick <player>") ==
          "name=kick literals=[kick] args=[player/word/false/null/null] usage=kick <player: word> "
          "permission=basalt.arena");

    // Extra spaces everywhere the tokenizer has to cope with.
    CHECK(describe("  arena   create   < name :  word >   [ size : int  8..64 ]  ") ==
          "name=arena literals=[arena, create] args=[name/word/false/null/null, size/int/true/8/64] "
          "usage=arena create <name: word> [size: int 8..64] permission=basalt.arena");

    // No space between the name and the first argument, which the tokenizer has to split.
    CHECK(describe("arena<name: word>") ==
          "name=arena literals=[arena] args=[name/word/false/null/null] usage=arena <name: word> "
          "permission=basalt.arena");

    // Two optionals in a row, which is allowed.
    CHECK(describe("warp <name: word> [x: int] [y: int]") ==
          "name=warp literals=[warp] args=[name/word/false/null/null, x/int/true/null/null, y/int/true/null/null] "
          "usage=warp <name: word> [x: int] [y: int] permission=basalt.arena");
}

TEST_CASE("a range with one end, or with no numbers in it at all") {
    // Half a range is still a range, and what was left out comes back as it went in.
    CHECK(describe("give <amount: int 1..>") ==
          "name=give literals=[give] args=[amount/int/false/1/null] usage=give <amount: int 1..null> "
          "permission=basalt.arena");
    CHECK(describe("give <amount: int ..99>") ==
          "name=give literals=[give] args=[amount/int/false/null/99] usage=give <amount: int null..99> "
          "permission=basalt.arena");
    // A number with something stuck to it is not a number, not a number and some rubbish.
    CHECK(describe("give <amount: int 8x..64>") ==
          "name=give literals=[give] args=[amount/int/false/null/64] usage=give <amount: int null..64> "
          "permission=basalt.arena");
    // Neither end is a number, so there is no range and the usage does not pretend there was one.
    CHECK(describe("give <amount: int a..b>") ==
          "name=give literals=[give] args=[amount/int/false/null/null] usage=give <amount: int> "
          "permission=basalt.arena");
}

TEST_CASE("a spec the loader cannot work with is refused, and says why") {
    CHECK(refusal("") == "a_command_with_no_name");
    CHECK(refusal("   ") == "a_command_with_no_name");
    CHECK(refusal("<name: word>") == "'<name:_word>'_starts_with_an_argument_instead_of_a_name");
    CHECK(refusal("arena <name: word> create") ==
          "'arena_<name:_word>_create'_puts_the_word_'create'_after_an_argument");
    CHECK(refusal("arena [size: int] <name: word>") ==
          "'arena_[size:_int]_<name:_word>'_has_the_required_'name'_after_an_optional_one");
    CHECK(refusal("arena <name: word") == "'arena_<name:_word'_has_an_argument_that_never_closes");
    CHECK(refusal("arena [size: int>") == "'arena_[size:_int>'_has_an_argument_that_never_closes");
    CHECK(refusal("arena <: word>") == "'arena_<:_word>'_has_an_argument_with_no_name");
    // A type the server does not have yet fails at load, not at the first use.
    CHECK(refusal("arena <name: player>") ==
          "'arena_<name:_player>'_asks_for_the_argument_type_'player',_which_this_server_does_not_have_yet");
}

TEST_CASE("the argument types a plugin can name") {
    CHECK(spellingOf(PluginArgumentType::Word) == "word");
    CHECK(spellingOf(PluginArgumentType::Text) == "text");
    CHECK(spellingOf(PluginArgumentType::Int) == "int");
    CHECK(spellingOf(PluginArgumentType::Float) == "float");
    CHECK(spellingOf(PluginArgumentType::Bool) == "bool");
    CHECK(argumentTypeOf("int") == PluginArgumentType::Int);
    CHECK_FALSE(argumentTypeOf("player").has_value());
}
