#include <basalt/server/plugins/PluginJson.hpp>

#include <charconv>
#include <cstdint>
#include <format>

namespace basalt::server::plugins::pluginJson {
namespace {

void writeValue(lua::LuaArgs& lua, std::int32_t index, std::string& out);

void writeString(std::string_view value, std::string& out) {
    out += '"';
    for (const char character : value) {
        switch (character) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                out += std::format("\\u{:04x}", static_cast<unsigned char>(character));
            } else {
                out += character;
            }
            break;
        }
    }
    out += '"';
}

void writeTable(lua::LuaArgs& lua, std::int32_t index, std::string& out) {
    const std::int64_t length = lua.length(index);
    // 1..n and nothing else is an array; anything else is an object, which is the one place a Lua
    // table and JSON disagree about what they are.
    bool isArray = length > 0;
    if (isArray) {
        lua.pushIndex(index, length + 1);
        isArray = lua.isNil(-1);
        lua.pop(1);
    }
    if (isArray) {
        out += '[';
        for (std::int64_t position = 1; position <= length; ++position) {
            if (position > 1) {
                out += ',';
            }
            lua.pushIndex(index, position);
            writeValue(lua, -1, out);
            lua.pop(1);
        }
        out += ']';
        return;
    }

    out += '{';
    bool first = true;
    lua.pushNil();
    // `next` leaves the key and the value; the key has to stay for the following call, so only the
    // value is dropped each time round.
    while (lua.nextEntry(index < 0 ? index - 1 : index)) {
        if (!first) {
            out += ',';
        }
        first = false;
        // `luaL_tolstring` pushes the text rather than converting the value where it lies, which is
        // why the key survives to be handed back to `next` on the way round.
        writeString(lua.toText(-2), out);
        out += ':';
        writeValue(lua, -1, out);
        lua.pop(1);
    }
    out += '}';
}

void writeValue(lua::LuaArgs& lua, std::int32_t index, std::string& out) {
    if (lua.isNil(index)) {
        out += "null";
        return;
    }
    if (lua.isBoolean(index)) {
        out += lua.toBoolean(index) ? "true" : "false";
        return;
    }
    if (lua.isInteger(index)) {
        out += std::to_string(lua.toInteger(index));
        return;
    }
    if (lua.isNumber(index)) {
        out += std::format("{}", lua.toNumber(index));
        return;
    }
    if (lua.isTable(index)) {
        writeTable(lua, index, out);
        return;
    }
    writeString(lua.toText(index), out);
}

/// A reader over the document, leaving what it makes on the Lua stack.
class Reader {
public:
    Reader(lua::LuaArgs& lua, std::string_view text) : lua_(&lua), text_(text) {}

    void skipSpace() {
        while (index_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[index_])) != 0) {
            ++index_;
        }
    }

    void value() {
        skipSpace();
        if (index_ >= text_.size()) {
            lua_->pushNil();
            return;
        }
        switch (text_[index_]) {
        case '{':
            object();
            return;
        case '[':
            array();
            return;
        case '"':
            lua_->pushString(string());
            return;
        case 't':
            literal("true");
            lua_->pushBoolean(true);
            return;
        case 'f':
            literal("false");
            lua_->pushBoolean(false);
            return;
        case 'n':
            literal("null");
            lua_->pushNil();
            return;
        default:
            number();
            return;
        }
    }

private:
    void literal(std::string_view word) {
        if (text_.substr(index_).starts_with(word)) {
            index_ += word.size();
        } else {
            index_ = text_.size();
        }
    }

    void object() {
        lua_->pushTable();
        ++index_;
        while (true) {
            skipSpace();
            if (index_ >= text_.size() || text_[index_] == '}') {
                ++index_;
                return;
            }
            const std::string key = string();
            skipSpace();
            if (index_ < text_.size() && text_[index_] == ':') {
                ++index_;
            }
            value();
            lua_->setField(key);
            skipSpace();
            if (index_ < text_.size() && text_[index_] == ',') {
                ++index_;
            }
        }
    }

    void array() {
        lua_->pushTable();
        ++index_;
        std::int64_t position = 1;
        while (true) {
            skipSpace();
            if (index_ >= text_.size() || text_[index_] == ']') {
                ++index_;
                return;
            }
            value();
            lua_->setIndex(position++);
            skipSpace();
            if (index_ < text_.size() && text_[index_] == ',') {
                ++index_;
            }
        }
    }

    [[nodiscard]] std::string string() {
        std::string out;
        if (index_ < text_.size() && text_[index_] == '"') {
            ++index_;
        }
        while (index_ < text_.size() && text_[index_] != '"') {
            const char character = text_[index_];
            if (character == '\\' && index_ + 1 < text_.size()) {
                ++index_;
                switch (text_[index_]) {
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u': {
                    const std::string_view digits =
                        text_.substr(index_ + 1, std::min<std::size_t>(4, text_.size() - index_ - 1));
                    std::int32_t code = 0;
                    if (std::from_chars(digits.data(), digits.data() + digits.size(), code, 16).ec == std::errc{}) {
                        out += static_cast<char>(code);
                    }
                    index_ += 4;
                    break;
                }
                default:
                    out += text_[index_];
                    break;
                }
            } else {
                out += character;
            }
            ++index_;
        }
        ++index_;
        return out;
    }

    void number() {
        const std::size_t start = index_;
        while (index_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[index_])) != 0 ||
                                         std::string_view{"-+.eE"}.find(text_[index_]) != std::string_view::npos)) {
            ++index_;
        }
        const std::string_view raw = text_.substr(start, index_ - start);
        std::int64_t whole = 0;
        if (std::from_chars(raw.data(), raw.data() + raw.size(), whole).ec == std::errc{} &&
            raw.find_first_of(".eE") == std::string_view::npos) {
            lua_->pushInteger(whole);
            return;
        }
        double real = 0.0;
        if (std::from_chars(raw.data(), raw.data() + raw.size(), real).ec == std::errc{}) {
            lua_->pushNumber(real);
            return;
        }
        lua_->pushNil();
    }

    lua::LuaArgs* lua_;
    std::string_view text_;
    std::size_t index_ = 0;
};

} // namespace

std::string write(lua::LuaArgs& lua, std::int32_t index) {
    std::string out;
    writeValue(lua, index, out);
    return out;
}

void parse(lua::LuaArgs& lua, std::string_view text) {
    Reader reader{lua, text};
    reader.skipSpace();
    reader.value();
    if (!lua.isTable(-1)) {
        lua.pop(1);
        lua.pushTable();
    }
}

} // namespace basalt::server::plugins::pluginJson
