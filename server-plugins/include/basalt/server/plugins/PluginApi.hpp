#pragma once

#include <basalt/server/plugins/PluginHost.hpp>

#include <cstdint>

namespace basalt::server::plugins::pluginApi {
void install(PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host, std::int32_t lanes);
} // namespace basalt::server::plugins::pluginApi
