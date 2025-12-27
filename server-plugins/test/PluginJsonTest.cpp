#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginJson.hpp>

#include <doctest.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

using namespace basalt::server::plugins;
using namespace basalt::server::lua;

namespace {

/// A double is printed with a decimal point always present.
[[nodiscard]] std::string number(double value) {
    std::string text = std::format("{}", value);
    if (text.find('.') == std::string::npos && text.find('e') == std::string::npos) {
        text += ".0";
    }
    return text;
}

/// Every value in the tree, keys sorted, so two interpreters can be compared at all: the order an
/// object's keys come out in is whatever `next` gives, and the two do not agree.
[[nodiscard]] std::string describe(LuaArgs& lua, std::int32_t index);

[[nodiscard]] std::string describeTable(LuaArgs& lua, std::int32_t index) {
    std::vector<std::string> entries;
    lua.pushNil();
    while (lua.nextEntry(index < 0 ? index - 1 : index)) {
        // The value first, because describing the key must not disturb what `next` holds on to.
        const std::string value = describe(lua, -1);
        const std::string key = describe(lua, -2);
        entries.push_back(key + "=" + value);
        lua.pop(1);
    }
    std::sort(entries.begin(), entries.end());
    std::string out = "{";
    for (const std::string& entry : entries) {
        if (out.size() > 1) {
            out += ',';
        }
        out += entry;
    }
    return out + "}";
}

std::string describe(LuaArgs& lua, std::int32_t index) {
    if (lua.isNil(index)) {
        return "nil";
    }
    if (lua.isBoolean(index)) {
        return lua.toBoolean(index) ? "true" : "false";
    }
    if (lua.isInteger(index)) {
        return "i" + std::to_string(lua.toInteger(index));
    }
    if (lua.isNumber(index)) {
        return "n" + number(lua.toNumber(index));
    }
    if (lua.isTable(index)) {
        return describeTable(lua, index);
    }
    return "s" + lua.toText(index);
}

/// A state to build values in, with the document of the round-trip cases already on the stack.
class Fixture {
public:
    Fixture() {
        sandbox_ = LuaSandbox::create("arena");
        lua_ = std::make_unique<LuaArgs>(sandbox_->state());
    }

    [[nodiscard]] LuaArgs& lua() { return *lua_; }

    /// Pushes the same document the oracle builds.
    void pushDocument() {
        lua_->pushTable();
        lua_->pushString("Arena");
        lua_->setField("name");
        lua_->pushInteger(24);
        lua_->setField("players");
        lua_->pushNumber(1.5);
        lua_->setField("rate");
        lua_->pushBoolean(true);
        lua_->setField("open");
        lua_->pushBoolean(false);
        lua_->setField("closed");
        lua_->pushTable();
        lua_->pushString("arena");
        lua_->setIndex(1);
        lua_->pushString("lobby");
        lua_->setIndex(2);
        lua_->setField("worlds");
        lua_->pushTable();
        lua_->pushInteger(2);
        lua_->setField("depth");
        lua_->setField("nested");
        lua_->pushTable();
        lua_->setField("empty");
    }

private:
    std::unique_ptr<LuaSandbox> sandbox_;
    std::unique_ptr<LuaArgs> lua_;
};

} // namespace

TEST_CASE("what a plugin writes as json is what the frozen kotlin writes") {
    Fixture fixture;
    LuaArgs& lua = fixture.lua();

    // An array's text is defined, because its order is.
    lua.pushTable();
    lua.pushInteger(1);
    lua.setIndex(1);
    lua.pushString("two");
    lua.setIndex(2);
    lua.pushBoolean(true);
    lua.setIndex(3);
    CHECK(pluginJson::write(lua, -1) == R"([1,"two",true])");
    lua.pop(1);

    lua.pushInteger(7);
    CHECK(pluginJson::write(lua, -1) == "7");
    lua.pop(1);
    lua.pushNumber(7.5);
    CHECK(pluginJson::write(lua, -1) == "7.5");
    lua.pop(1);
    lua.pushBoolean(true);
    CHECK(pluginJson::write(lua, -1) == "true");
    lua.pop(1);
    lua.pushNil();
    CHECK(pluginJson::write(lua, -1) == "null");
    lua.pop(1);

    // Everything the writer has to escape, in one string, including a bare control character.
    lua.pushString(std::string{"a\"b\\c"} + '\n' + "d" + '\r' + "e" + '\t' + "f" + '\x01' + "g");
    CHECK(pluginJson::write(lua, -1) == R"("a\"b\\c\nd\re\tf\u0001g")");
    lua.pop(1);

    lua.pushTable();
    lua.pushTable();
    lua.pushInteger(3);
    lua.setIndex(1);
    lua.pushInteger(4);
    lua.setIndex(2);
    lua.setIndex(1);
    CHECK(pluginJson::write(lua, -1) == "[[3,4]]");
    lua.pop(1);

    // Past what a double can hold exactly, which is the only place writing an integer as a number
    // would be visible.
    lua.pushInteger(9007199254740993LL);
    CHECK(pluginJson::write(lua, -1) == "9007199254740993");
    lua.pop(1);
}

TEST_CASE("an object's content survives a write, whatever order its keys come out in") {
    Fixture fixture;
    LuaArgs& lua = fixture.lua();
    fixture.pushDocument();

    const std::string written = pluginJson::write(lua, -1);
    lua.pop(1);
    pluginJson::parse(lua, written);
    CHECK(describe(lua, -1) ==
          "{sclosed=false,sempty={},sname=sArena,snested={sdepth=i2},sopen=true,splayers=i24,srate=n1.5,"
          "sworlds={i1=sarena,i2=slobby}}");
    lua.pop(1);
}

TEST_CASE("what a plugin reads back is what the frozen kotlin reads back") {
    Fixture fixture;
    LuaArgs& lua = fixture.lua();
    const auto parsed = [&](std::string_view text) {
        pluginJson::parse(lua, text);
        const std::string described = describe(lua, -1);
        lua.pop(1);
        return described;
    };

    CHECK(parsed(R"({"name":"Arena","players":24,"rate":1.5,"open":true,"missing":null})") ==
          "{sname=sArena,sopen=true,splayers=i24,srate=n1.5}");
    CHECK(parsed(R"([1,"two",true,[3,4],{"k":"v"}])") == "{i1=i1,i2=stwo,i3=true,i4={i1=i3,i2=i4},i5={sk=sv}}");
    CHECK(parsed(R"({"a":{"b":{"c":[1,2,3]}}})") == "{sa={sb={sc={i1=i1,i2=i2,i3=i3}}}}");
    CHECK(parsed(R"({"text":"a\"b\\c\nd\re\tf\u0041g"})") ==
          std::string{"{stext=sa\"b\\c"} + '\n' + "d" + '\r' + "e" + '\t' + "fAg}");
    CHECK(parsed("  {  \"a\" :  1 ,  \"b\" : [ 2 , 3 ]  }  ") == "{sa=i1,sb={i1=i2,i2=i3}}");
    CHECK(parsed("{}") == "{}");
    CHECK(parsed("[]") == "{}");
    // A number keeps its kind: 7 stays an integer, which is what makes a saved file readable.
    CHECK(parsed(R"({"whole":7,"negative":-12,"real":1.25,"exponent":2e3,"zero":0})") ==
          "{sexponent=n2000.0,snegative=i-12,sreal=n1.25,swhole=i7,szero=i0}");
    // Not a document at all: an empty table rather than a failure.
    CHECK(parsed("42") == "{}");
    CHECK(parsed("") == "{}");
    // Truncated, which a saved file can be after a crash: what was read stays read.
    CHECK(parsed(R"({"a":1,"b":)") == "{sa=i1}");
    // A literal that starts like one and is not: the reader gives up rather than reading past it
    // forever, and what it had already read stays read.
    CHECK(parsed(R"({"a":tXue,"b":2})") == "{sa=true}");
}
