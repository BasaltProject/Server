#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>

namespace basalt::server::plugins {

/// A packet as Lua sees it.
///
/// Deliberately thin: name, who it is about, the bytes, and whether it goes on. The fields of each
/// packet by name are the next thing this grows, and until then `bytes` is the honest answer rather
/// than a half-decoded table that would be wrong for most of the hundred and thirty six.
void pushPacket(lua::LuaArgs& lua, PluginPacket& packet, PluginHost& host, LoadedPlugin& plugin);

} // namespace basalt::server::plugins
