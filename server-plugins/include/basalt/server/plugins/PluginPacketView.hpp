#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/wire/lua/LuaArgs.hpp>

namespace basalt::server::plugins {
void pushPacket(wire::lua::LuaArgs& lua, PluginPacket& packet, PluginHost& host, LoadedPlugin& plugin);
} // namespace basalt::server::plugins
