#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/StringReader.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using basalt::server::runtime::command::CommandSyntaxError;
using basalt::server::runtime::command::StringReader;

namespace {

[[nodiscard]] std::string messageOf(const CommandSyntaxError& error) {
    return basalt::wire::text::toPlainText(error.component);
}

} // namespace

TEST_CASE("a reader walks the line and says how much of it is left") {
    StringReader reader{"hello world"};

    CHECK(reader.string() == "hello world");
    CHECK(reader.cursor() == 0);
    CHECK(reader.remaining() == "hello world");
    CHECK(reader.remainingLength() == 11);
    CHECK(reader.canRead());
    CHECK(reader.canRead(11));
    CHECK_FALSE(reader.canRead(12));
    CHECK(reader.peek() == 'h');
    CHECK(reader.peek(6) == 'w');

    CHECK(reader.read() == 'h');
    CHECK(reader.cursor() == 1);
    reader.skip();
    CHECK(reader.remaining() == "llo world");

    reader.setCursor(11);
    CHECK_FALSE(reader.canRead());
    CHECK(reader.remaining().empty());
    CHECK(reader.remainingLength() == 0);
}

TEST_CASE("whitespace is skipped, and only whitespace") {
    // parity: `Character.isWhitespace`, all six of the ones a command line can carry.
    StringReader reader{" \t\n\r\f\vone"};

    reader.skipWhitespace();

    CHECK(reader.remaining() == "one");
    reader.skipWhitespace();
    CHECK(reader.cursor() == 6);

    for (const char other : {'a', '0', '_', '!', '\0'}) {
        StringReader stopped{std::string{other} + " x"};
        stopped.skipWhitespace();
        CHECK(stopped.cursor() == 0);
    }
}

TEST_CASE("an expected symbol is consumed, and a missing one is named") {
    StringReader reader{"[1]"};

    CHECK(reader.expect('[').has_value());
    CHECK(reader.cursor() == 1);

    const std::expected<void, CommandSyntaxError> wrong = reader.expect(']');
    REQUIRE_FALSE(wrong.has_value());
    CHECK(messageOf(wrong.error()) == "Expected ']'");
    CHECK(reader.cursor() == 1);
    CHECK(wrong.error().cursor == 1);
    CHECK(wrong.error().input == "[1]");

    StringReader done{""};
    const std::expected<void, CommandSyntaxError> empty = done.expect('[');
    REQUIRE_FALSE(empty.has_value());
    CHECK(messageOf(empty.error()) == "Expected '['");
}

TEST_CASE("an unquoted word stops at the first character that is not one") {
    StringReader reader{"stone_slab+2.5-x other"};
    CHECK(reader.readUnquotedString() == "stone_slab+2.5-x");
    CHECK(reader.remaining() == " other");

    StringReader colon{"minecraft:stone"};
    CHECK(colon.readUnquotedString() == "minecraft");

    StringReader none{"!!!"};
    CHECK(none.readUnquotedString().empty());
    CHECK(none.cursor() == 0);
}

TEST_CASE("a namespaced id takes the colon and the slash too") {
    StringReader reader{"minecraft:blocks/stone rest"};

    CHECK(reader.readIdentifier() == "minecraft:blocks/stone");
    CHECK(reader.remaining() == " rest");
}

TEST_CASE("a literal has to be followed by a space or the end of the line") {
    StringReader exact{">="};
    CHECK(exact.readLiteral(">="));
    CHECK(exact.cursor() == 2);

    StringReader spaced{">= 3"};
    CHECK(spaced.readLiteral(">="));
    CHECK(spaced.remaining() == " 3");

    // parity: `>=` must not match the `>` of `>=`, which is what the trailing test is for.
    StringReader longer{">=3"};
    CHECK_FALSE(longer.readLiteral(">"));
    CHECK(longer.cursor() == 0);

    StringReader other{"less"};
    CHECK_FALSE(other.readLiteral("more"));
    CHECK(other.cursor() == 0);
}

TEST_CASE("a quoted string is read between its quotes, either kind") {
    StringReader doubled{R"("hello world" rest)"};
    const std::expected<std::string, CommandSyntaxError> read = doubled.readString();
    REQUIRE(read.has_value());
    CHECK(*read == "hello world");
    CHECK(doubled.remaining() == " rest");

    StringReader singled{"'hi'"};
    CHECK(singled.readString().value() == "hi");

    StringReader plain{"word rest"};
    CHECK(plain.readString().value() == "word");

    StringReader empty{""};
    CHECK(empty.readString().value().empty());
}

TEST_CASE("a quote can be escaped, and only a quote or a backslash can") {
    StringReader escaped{R"("say \"hi\" and \\ back")"};
    CHECK(escaped.readString().value() == R"(say "hi" and \ back)");

    StringReader bad{R"("say \n")"};
    const std::expected<std::string, CommandSyntaxError> wrong = bad.readString();
    REQUIRE_FALSE(wrong.has_value());
    CHECK(messageOf(wrong.error()) == "Invalid escape sequence 'n' in quoted string");
    CHECK(bad.string()[static_cast<std::size_t>(wrong.error().cursor)] == 'n');

    StringReader unclosed{R"("never ends)"};
    const std::expected<std::string, CommandSyntaxError> open = unclosed.readString();
    REQUIRE_FALSE(open.has_value());
    CHECK(messageOf(open.error()) == "Unclosed quoted string");

    StringReader mixed{R"('say "hi"')"};
    CHECK(mixed.readString().value() == R"(say "hi")");
}

TEST_CASE("an integer is read, and a word that is not one is named") {
    StringReader positive{"42 rest"};
    CHECK(positive.readInt().value() == 42);
    CHECK(positive.remaining() == " rest");

    StringReader negative{"-7"};
    CHECK(negative.readInt().value() == -7);

    StringReader missing{"abc"};
    const std::expected<std::int32_t, CommandSyntaxError> none = missing.readInt();
    REQUIRE_FALSE(none.has_value());
    CHECK(messageOf(none.error()) == "Expected integer");

    // parity: what the number reader consumes and what parses as an integer are not the same set.
    StringReader broken{"1.5"};
    const std::expected<std::int32_t, CommandSyntaxError> bad = broken.readInt();
    REQUIRE_FALSE(bad.has_value());
    CHECK(messageOf(bad.error()) == "Invalid integer '1.5'");
    CHECK(broken.cursor() == 0);
}

TEST_CASE("a double is read, exponent and all") {
    StringReader plain{"1.5 rest"};
    CHECK(plain.readDouble().value() == doctest::Approx(1.5));
    CHECK(plain.remaining() == " rest");

    StringReader whole{"-3"};
    CHECK(whole.readDouble().value() == doctest::Approx(-3.0));

    StringReader exponent{"1e3"};
    CHECK(exponent.readDouble().value() == doctest::Approx(1000.0));

    StringReader missing{"abc"};
    const std::expected<double, CommandSyntaxError> none = missing.readDouble();
    REQUIRE_FALSE(none.has_value());
    CHECK(messageOf(none.error()) == "Expected double");

    StringReader broken{"1.2.3"};
    const std::expected<double, CommandSyntaxError> bad = broken.readDouble();
    REQUIRE_FALSE(bad.has_value());
    CHECK(messageOf(bad.error()) == "Invalid double '1.2.3'");
    CHECK(broken.cursor() == 0);
}

TEST_CASE("an error carries the line and where the reader stopped") {
    StringReader reader{"give @p stone"};
    reader.setCursor(8);

    const CommandSyntaxError error = reader.syntaxError("Something went wrong");

    CHECK(messageOf(error) == "Something went wrong");
    CHECK(error.input == "give @p stone");
    CHECK(error.cursor == 8);

    const CommandSyntaxError plain;
    CHECK(plain.cursor == -1);
    CHECK_FALSE(plain.input.has_value());
}

TEST_CASE("the characters a word is made of are exactly brigadier's own") {
    StringReader all{"09AZaz_-.+"};
    CHECK(all.readUnquotedString() == "09AZaz_-.+");

    for (const char outside : {'/', ':', '@', '[', '`', '{', ' ', '!', '"'}) {
        StringReader stopped{std::string{"ab"} + outside + "cd"};
        CHECK(stopped.readUnquotedString() == "ab");
    }
}

TEST_CASE("the characters a number is made of are exactly brigadier's own") {
    StringReader all{"-90.5e+9"};
    CHECK(all.readDouble().value() == doctest::Approx(-90.5e9));
    CHECK(all.cursor() == 8);
    StringReader capital{"1E2"};
    CHECK(capital.readDouble().value() == doctest::Approx(100.0));

    for (const char outside : {'/', ':', 'a', 'd', 'f', 'D', 'F', ' ', ','}) {
        StringReader stopped{std::string{"12"} + outside + "34"};
        CHECK(stopped.readInt().value() == 12);
        CHECK(stopped.cursor() == 2);
    }
}
