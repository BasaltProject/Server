#include <basalt/server/runtime/command/StringReader.hpp>

#include <charconv>

namespace basalt::server::runtime::command {
namespace {

[[nodiscard]] bool isAllowedInUnquotedString(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-' ||
           c == '.' || c == '+';
}

[[nodiscard]] bool isAllowedInIdentifier(char c) { return isAllowedInUnquotedString(c) || c == ':' || c == '/'; }

[[nodiscard]] bool isAllowedInNumber(char c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '+' || c == 'e' || c == 'E';
}

/// parity: `Character.isWhitespace`, of which a command line only ever carries these.
[[nodiscard]] bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

} // namespace

std::string StringReader::remaining() const { return string_.substr(static_cast<std::size_t>(cursor_)); }

std::int32_t StringReader::remainingLength() const noexcept {
    return static_cast<std::int32_t>(string_.size()) - cursor_;
}

bool StringReader::canRead(std::int32_t offset) const noexcept {
    return cursor_ + offset <= static_cast<std::int32_t>(string_.size());
}

char StringReader::peek(std::int32_t offset) const { return string_[static_cast<std::size_t>(cursor_ + offset)]; }

char StringReader::read() { return string_[static_cast<std::size_t>(cursor_++)]; }

void StringReader::skipWhitespace() {
    while (canRead() && isWhitespace(peek())) {
        skip();
    }
}

Parsed<void> StringReader::expect(char symbol) {
    if (!canRead() || peek() != symbol) {
        return std::unexpected(syntaxError(std::string{"Expected '"} + symbol + "'"));
    }
    skip();
    return {};
}

std::string StringReader::readUnquotedString() {
    const std::int32_t start = cursor_;
    while (canRead() && isAllowedInUnquotedString(peek())) {
        skip();
    }
    return string_.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(cursor_ - start));
}

bool StringReader::readLiteral(std::string_view literal) {
    if (string_.compare(static_cast<std::size_t>(cursor_), literal.size(), literal) != 0) {
        return false;
    }
    const std::size_t end = static_cast<std::size_t>(cursor_) + literal.size();
    if (end < string_.size() && string_[end] != ' ') {
        return false;
    }
    cursor_ = static_cast<std::int32_t>(end);
    return true;
}

Parsed<std::string> StringReader::readString() {
    if (!canRead()) {
        return std::string{};
    }
    const char next = peek();
    if (next == '"' || next == '\'') {
        skip();
        return readStringUntil(next);
    }
    return readUnquotedString();
}

std::string StringReader::readIdentifier() {
    const std::int32_t start = cursor_;
    while (canRead() && isAllowedInIdentifier(peek())) {
        skip();
    }
    return string_.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(cursor_ - start));
}

Parsed<std::int32_t> StringReader::readInt() {
    const std::int32_t start = cursor_;
    while (canRead() && isAllowedInNumber(peek())) {
        skip();
    }
    const std::string text = string_.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(cursor_ - start));
    if (text.empty()) {
        return std::unexpected(syntaxError("Expected integer"));
    }
    std::int32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        cursor_ = start;
        return std::unexpected(syntaxError("Invalid integer '" + text + "'"));
    }
    return value;
}

Parsed<double> StringReader::readDouble() {
    const std::int32_t start = cursor_;
    while (canRead() && isAllowedInNumber(peek())) {
        skip();
    }
    const std::string text = string_.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(cursor_ - start));
    if (text.empty()) {
        return std::unexpected(syntaxError("Expected double"));
    }
    double value = 0.0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        cursor_ = start;
        return std::unexpected(syntaxError("Invalid double '" + text + "'"));
    }
    return value;
}

Parsed<std::string> StringReader::readStringUntil(char terminator) {
    std::string result;
    bool escaped = false;
    while (canRead()) {
        const char next = read();
        if (escaped) {
            if (next != terminator && next != '\\') {
                --cursor_;
                return std::unexpected(
                    syntaxError(std::string{"Invalid escape sequence '"} + next + "' in quoted string"));
            }
            result.push_back(next);
            escaped = false;
            continue;
        }
        if (next == '\\') {
            escaped = true;
            continue;
        }
        if (next == terminator) {
            return result;
        }
        result.push_back(next);
    }
    return std::unexpected(syntaxError("Unclosed quoted string"));
}

CommandSyntaxError StringReader::syntaxError(const std::string& message) const {
    return CommandSyntaxError{.component = wire::text::Component::text(message), .input = string_, .cursor = cursor_};
}

} // namespace basalt::server::runtime::command
