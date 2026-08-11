#include <basalt/server/runtime/world/ColourCodes.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace basalt::server::runtime::world::colourCodes {
namespace {

struct Code {
    char letter;
    std::string_view tag;
};

constexpr std::array<Code, 22> kCodes{
    {{'0', "black"},       {'1', "dark_blue"},  {'2', "dark_green"}, {'3', "dark_aqua"},     {'4', "dark_red"},
     {'5', "dark_purple"}, {'6', "gold"},       {'7', "gray"},       {'8', "dark_gray"},     {'9', "blue"},
     {'a', "green"},       {'b', "aqua"},       {'c', "red"},        {'d', "light_purple"},  {'e', "yellow"},
     {'f', "white"},       {'k', "obfuscated"}, {'l', "bold"},       {'m', "strikethrough"}, {'n', "underlined"},
     {'o', "italic"},      {'r', "reset"}}};

[[nodiscard]] const Code* codeOf(char letter) {
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    for (const Code& code : kCodes) {
        if (code.letter == lower) {
            return &code;
        }
    }
    return nullptr;
}

[[nodiscard]] bool isHex(char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

} // namespace

bool hasCodes(std::string_view text) {
    for (std::size_t at = 0; at + 1 < text.size(); ++at) {
        if (text[at] == '&' && (codeOf(text[at + 1]) != nullptr || text[at + 1] == '#')) {
            return true;
        }
    }
    return false;
}

wire::text::Component ofAmpersands(std::string_view text) {
    std::string mini;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const char c = text[at];
        if (c == '&' && at + 8 <= text.size() && text[at + 1] == '#' &&
            std::ranges::all_of(text.substr(at + 2, 6), isHex)) {
            mini += "<reset><#" + std::string{text.substr(at + 2, 6)} + ">";
            at += 7;
        } else if (c == '&' && at + 1 < text.size() && codeOf(text[at + 1]) != nullptr) {
            const Code* code = codeOf(text[at + 1]);
            mini += (isHex(code->letter) ? "<reset>" : "") + std::string{"<"} + std::string{code->tag} + ">";
            ++at;
        } else if (c == '<' || c == '\\') {
            mini += '\\';
            mini += c;
        } else {
            mini += c;
        }
    }
    return wire::text::miniMessage::parse(mini);
}

} // namespace basalt::server::runtime::world::colourCodes
