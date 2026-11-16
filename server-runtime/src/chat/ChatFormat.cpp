#include <basalt/server/runtime/chat/ChatFormat.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <string_view>

namespace basalt::server::runtime::chat {
namespace {

std::string escaped(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (c == '<') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

void fill(std::string& format, const std::string_view token, const std::string& value) {
    for (std::size_t at = format.find(token); at != std::string::npos; at = format.find(token, at)) {
        format.replace(at, token.size(), value);
        at += value.size();
    }
}

} // namespace

wire::text::Component formatLine(std::string format, const std::string& player, const std::string& message) {
    fill(format, "{player}", escaped(player));
    fill(format, "{message}", escaped(message));
    return wire::text::miniMessage::parse(format);
}

} // namespace basalt::server::runtime::chat
