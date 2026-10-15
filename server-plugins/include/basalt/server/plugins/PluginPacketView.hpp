#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>

namespace basalt::server::plugins {
void pushPacket(lua::LuaArgs& lua, PluginPacket& packet, PluginHost& host, LoadedPlugin& plugin);
} // namespace basalt::server::plugins
