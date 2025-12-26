#include <basalt/server/lua/LuaCall.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <utility>

namespace basalt::server::lua {
namespace {

/// Where the call a thread belongs to lives, keyed by the thread itself.
constexpr const char* kCallsKey = "basalt.calls";

struct HostBody {
    std::function<std::int32_t(LuaArgs&)> immediate;
    LuaCall::Arrange arrange;
    std::string name;
};

int releaseHost(lua_State* state) {
    auto* body = static_cast<HostBody*>(lua_touserdata(state, 1));
    body->~HostBody();
    return 0;
}

[[nodiscard]] HostBody* bodyOf(lua_State* state) {
    return static_cast<HostBody*>(lua_touserdata(state, lua_upvalueindex(1)));
}

/// The userdata is created with a metatable that frees the body when the state closes, so a host
/// function's captured state has the same lifetime as the sandbox it was installed on.
[[nodiscard]] HostBody* newBody(lua_State* state) {
    void* memory = lua_newuserdatauv(state, sizeof(HostBody), 0);
    auto* body = new (memory) HostBody{};
    if (luaL_newmetatable(state, "basalt.hostbody") != 0) {
        lua_pushcfunction(state, &releaseHost);
        lua_setfield(state, -2, "__gc");
    }
    lua_setmetatable(state, -2);
    return body;
}

} // namespace

/// The one thing allowed to reach a call's privates from a C callback.
struct CallAccess {
    /// The call a thread is running under, or nothing when it is not running under one at all.
    ///
    /// What the state holds is a bare pointer, not a handle: what keeps the call alive while it is
    /// paused is the call's own `self_`, and a handle here as well would be a second owner that only
    /// a garbage collection could let go of.
    [[nodiscard]] static LuaCall* of(lua_State* thread) {
        lua_getfield(thread, LUA_REGISTRYINDEX, kCallsKey);
        lua_pushthread(thread);
        lua_rawget(thread, -2);
        auto* call = static_cast<LuaCall*>(lua_touserdata(thread, -1));
        lua_pop(thread, 2);
        return call;
    }

    static void remember(lua_State* thread, LuaCall* call) {
        lua_getfield(thread, LUA_REGISTRYINDEX, kCallsKey);
        lua_pushthread(thread);
        lua_pushlightuserdata(thread, call);
        lua_rawset(thread, -3);
        lua_pop(thread, 1);
    }

    /// Clears the entry as soon as the call is over. The weak keys would get there on their own at
    /// the next collection; this is what makes a finished thread's slot gone rather than pending.
    static void forget(lua_State* thread) {
        lua_getfield(thread, LUA_REGISTRYINDEX, kCallsKey);
        lua_pushthread(thread);
        lua_pushnil(thread);
        lua_rawset(thread, -3);
        lua_pop(thread, 1);
    }

    static int immediate(lua_State* state) {
        LuaArgs args{state};
        return bodyOf(state)->immediate(args);
    }

    /// Continuation for a waiting function: what runs when the plugin is resumed. There is nothing
    /// left to do, so it answers no values.
    static int afterWait(lua_State*, int, lua_KContext) { return 0; }

    static int waiting(lua_State* thread) {
        HostBody* body = bodyOf(thread);
        if (lua_isyieldable(thread) == 0) {
            return luaL_error(thread, "'%s' cannot be used from here: it needs to be able to wait", body->name.c_str());
        }
        LuaCall* call = of(thread);
        if (call == nullptr) {
            return luaL_error(thread, "'%s' cannot be used from here: it needs to be able to wait", body->name.c_str());
        }
        call->arranged_ = true;
        // The handle the scheduler is given owns the call: everything else about it is a bare
        // pointer, so this is what keeps a paused turn alive until somebody resumes it or drops it.
        body->arrange([held = call->shared_from_this()] {
            if (!held->finished_) {
                held->arranged_ = false;
                held->step(0);
            }
        });
        return lua_yieldk(thread, 0, 0, &afterWait);
    }
};

LuaCall::LuaCall(LuaSandbox& sandbox, lua_State* thread, std::int32_t threadRef, Done onDone)
    : sandbox_(&sandbox), thread_(thread), threadRef_(threadRef), onDone_(std::move(onDone)) {}

LuaCall::~LuaCall() {
    if (thread_ != nullptr) {
        luaL_unref(sandbox_->state(), LUA_REGISTRYINDEX, threadRef_);
    }
}

std::shared_ptr<LuaCall> LuaCall::start(LuaSandbox& sandbox, std::int32_t argumentCount, Done onDone) {
    lua_State* main = sandbox.state();
    lua_getfield(main, LUA_REGISTRYINDEX, kCallsKey);
    if (lua_isnil(main, -1)) {
        lua_pop(main, 1);
        lua_newtable(main);
        // Weak keys, or the table would be the one thing keeping every thread that ever ran alive:
        // it is an index from a running thread to its call, not a list of what has been.
        lua_newtable(main);
        lua_pushliteral(main, "k");
        lua_setfield(main, -2, "__mode");
        lua_setmetatable(main, -2);
        lua_setfield(main, LUA_REGISTRYINDEX, kCallsKey);
    } else {
        lua_pop(main, 1);
    }

    lua_State* thread = lua_newthread(main);
    // The thread is anchored in the registry: a paused call has nothing else holding it, and a
    // collected thread mid-pause is a plugin that silently stops.
    const std::int32_t threadRef = luaL_ref(main, LUA_REGISTRYINDEX);
    // The function and its arguments move from the sandbox's stack onto the thread's.
    lua_xmove(main, thread, argumentCount + 1);

    std::shared_ptr<LuaCall> call{new LuaCall{sandbox, thread, threadRef, std::move(onDone)}};
    CallAccess::remember(thread, call.get());
    sandbox.refill();
    call->step(argumentCount);
    return call;
}

void LuaCall::step(std::int32_t argumentCount) {
    int results = 0;
    const int status = lua_resume(thread_, sandbox_->state(), argumentCount, &results);
    if (status == LUA_YIELD) {
        return;
    }
    finished_ = true;
    std::optional<LuaFailure> failure;
    if (status != LUA_OK) {
        std::size_t length = 0;
        const char* text = lua_tolstring(thread_, -1, &length);
        failure =
            LuaFailure{.plugin = sandbox_->name(), .message = text == nullptr ? "error" : std::string{text, length}};
        lua_pop(thread_, 1);
    }
    CallAccess::forget(thread_);
    if (onDone_) {
        // Moved out first: the callback may drop the last reference to this call.
        const Done done = std::move(onDone_);
        onDone_ = nullptr;
        done(failure);
    }
}

namespace {

void installBody(LuaSandbox& sandbox, const std::string& name, HostBody built, lua_CFunction entry) {
    lua_State* state = sandbox.state();
    HostBody* body = newBody(state);
    *body = std::move(built);
    body->name = name;
    lua_pushcclosure(state, entry, 1);
    lua_setglobal(state, name.c_str());
}

} // namespace

void installHostFunction(LuaSandbox& sandbox, const std::string& name, std::function<std::int32_t(LuaArgs&)> body) {
    installBody(sandbox, name, HostBody{.immediate = std::move(body), .arrange = {}, .name = name},
                &CallAccess::immediate);
}

void installWaitingFunction(LuaSandbox& sandbox, const std::string& name, LuaCall::Arrange arrange) {
    installBody(sandbox, name, HostBody{.immediate = {}, .arrange = std::move(arrange), .name = name},
                &CallAccess::waiting);
}

std::expected<std::int32_t, LuaFailure> callSync(LuaSandbox& sandbox, const LuaRef& function,
                                                 const std::function<std::int32_t(LuaArgs&)>& pushArguments,
                                                 std::int32_t resultCount) {
    lua_State* state = sandbox.state();
    if (!function.valid()) {
        return std::unexpected(LuaFailure{.plugin = sandbox.name(), .message = "there is no function to call"});
    }
    const int before = lua_gettop(state);
    function.push();
    LuaArgs lua{state};
    const std::int32_t argumentCount = pushArguments ? pushArguments(lua) : 0;
    if (lua_pcall(state, argumentCount, resultCount, 0) != LUA_OK) {
        const LuaFailure failure = sandbox.failureFromStack();
        lua_settop(state, before);
        return std::unexpected(failure);
    }
    return lua_gettop(state) - before;
}

} // namespace basalt::server::lua
