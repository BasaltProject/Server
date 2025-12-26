#pragma once

#include <basalt/server/plugins/PluginHost.hpp>

#include <cstdint>

namespace basalt::server::plugins::pluginApi {

/// What a plugin sees when it starts: the whole of the API of this stage, put into its globals.
///
/// Everything registered here goes through the plugin's own ledger, so unloading it takes all of it
/// back out. Nothing is a singleton a plugin could reach around the host with.
void install(PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host, std::int32_t lanes);

} // namespace basalt::server::plugins::pluginApi
