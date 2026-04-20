#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginJson.hpp>
#include <basalt/wire/lua/LuaCall.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::plugins;
using namespace basalt::wire::lua;

namespace {
class Fixture {
public:
    Fixture() {
        sandbox_ = LuaSandbox::create("arena");
        lua_ = std::make_unique<LuaArgs>(sandbox_->state());
    }

    [[nodiscard]] LuaArgs& lua() { return *lua_; }

private:
    std::unique_ptr<LuaSandbox> sandbox_;
    std::unique_ptr<LuaArgs> lua_;
};
} // namespace

TEST_CASE("what a plugin writes as json is what the frozen kotlin writes") {
    Fixture fixture;
    LuaArgs& lua = fixture.lua();

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

    lua.pushInteger(9007199254740993LL);
    CHECK(pluginJson::write(lua, -1) == "9007199254740993");
    lua.pop(1);
}
