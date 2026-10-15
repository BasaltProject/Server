#pragma once

#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::command {

struct CommandSyntaxError {
    wire::text::Component component;
    std::optional<std::string> input;
    std::int32_t cursor = -1;
};

template <typename T>
using Parsed = std::expected<T, CommandSyntaxError>;

class StringReader {
public:
    explicit StringReader(std::string text) : string_(std::move(text)) {}

    [[nodiscard]] const std::string& string() const noexcept { return string_; }

    [[nodiscard]] std::int32_t cursor() const noexcept { return cursor_; }

    void setCursor(std::int32_t cursor) noexcept { cursor_ = cursor; }

    [[nodiscard]] std::string remaining() const;

    [[nodiscard]] std::int32_t remainingLength() const noexcept;

    [[nodiscard]] bool canRead(std::int32_t offset = 1) const noexcept;

    [[nodiscard]] char peek(std::int32_t offset = 0) const;

    char read();

    void skip() noexcept { ++cursor_; }

    void skipWhitespace();

    [[nodiscard]] Parsed<void> expect(char symbol);

    [[nodiscard]] std::string readUnquotedString();

    /// parity: `LiteralCommandNode.parse`.
    [[nodiscard]] bool readLiteral(std::string_view literal);

    [[nodiscard]] Parsed<std::string> readString();

    /// parity: `Identifier.read`.
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
