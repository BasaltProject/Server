#pragma once

#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::command {

/// A command that could not be parsed or refused to run.
///
/// `input` and `cursor` are what let the caller print vanilla's own context line, the grey prefix
/// with `<--[HERE]` marking where the parser stopped; a failure with no position (a command that
/// parsed and then declined) leaves the cursor at -1 and gets no such line, exactly like vanilla.
struct CommandSyntaxError {
    wire::text::Component component;
    std::optional<std::string> input;
    std::int32_t cursor = -1;
};

/// Whatever a reader answers, or why it could not.
template <typename T>
using Parsed = std::expected<T, CommandSyntaxError>;

/// A command line being consumed left to right, and the errors that come out of it.
///
/// Reimplements what brigadier's own `StringReader` does, error strings included: those strings are
/// observable, the client shows them verbatim under the command box, so "Expected integer" is
/// specification, not phrasing. They are plain literals in brigadier rather than translation keys,
/// which is why they are literals here too.
///
/// Brigadier throws; this answers `std::expected`, because a command a player typed wrong is an
/// ordinary outcome and not a failure of the server.
class StringReader {
public:
    explicit StringReader(std::string text) : string_(std::move(text)) {}

    [[nodiscard]] const std::string& string() const noexcept { return string_; }

    [[nodiscard]] std::int32_t cursor() const noexcept { return cursor_; }

    void setCursor(std::int32_t cursor) noexcept { cursor_ = cursor; }

    [[nodiscard]] std::string remaining() const;

    [[nodiscard]] std::int32_t remainingLength() const noexcept;

    [[nodiscard]] bool canRead(std::int32_t offset = 1) const noexcept;

    /// The character `offset` along. Only valid where `canRead(offset + 1)` says so.
    [[nodiscard]] char peek(std::int32_t offset = 0) const;

    char read();

    void skip() noexcept { ++cursor_; }

    void skipWhitespace();

    [[nodiscard]] Parsed<void> expect(char symbol);

    [[nodiscard]] std::string readUnquotedString();

    /// Consumes `literal` if it is what comes next, followed by a space or the end of the line.
    ///
    /// parity: `LiteralCommandNode.parse`, which matches text rather than a word: `>=` and `<` are
    /// literals in `/execute if score` and no word reader would ever read them.
    [[nodiscard]] bool readLiteral(std::string_view literal);

    /// A quoted string if the next character opens a quote, an unquoted word otherwise.
    [[nodiscard]] Parsed<std::string> readString();

    /// A namespaced id: the same characters an unquoted string allows, plus `:` and `/`.
    ///
    /// parity: `Identifier.read`, which is why `minecraft:oak_log` is one argument rather than
    /// stopping at the colon the way brigadier's own unquoted reader would.
    [[nodiscard]] std::string readIdentifier();

    [[nodiscard]] Parsed<std::int32_t> readInt();

    [[nodiscard]] Parsed<double> readDouble();

    [[nodiscard]] CommandSyntaxError syntaxError(const std::string& message) const;

private:
    [[nodiscard]] Parsed<std::string> readStringUntil(char terminator);

    std::string string_;
    std::int32_t cursor_ = 0;
};

} // namespace basalt::server::runtime::command
