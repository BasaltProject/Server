#pragma once

#include <basalt/server/runtime/plugins/ServerAccess.hpp>
#include <basalt/server/runtime/plugins/ServerAdvancements.hpp>
#include <basalt/server/runtime/plugins/ServerBossBars.hpp>
#include <basalt/server/runtime/plugins/ServerControl.hpp>
#include <basalt/server/runtime/plugins/ServerDatapacks.hpp>
#include <basalt/server/runtime/plugins/ServerLoot.hpp>
#include <basalt/server/runtime/plugins/ServerMaps.hpp>
#include <basalt/server/runtime/plugins/ServerScoreboards.hpp>
#include <basalt/server/runtime/plugins/ServerSelectors.hpp>
#include <basalt/server/runtime/plugins/ServerStructures.hpp>

#include <memory>

namespace basalt::server::runtime::plugins {

/// The plugin services of the smaller areas, built together so the server wires one object.
struct MiscServices {
    std::unique_ptr<ServerAccess> access;
    std::unique_ptr<ServerScoreboards> scoreboards;
    std::unique_ptr<ServerBossBars> bossBars;
    std::unique_ptr<ServerAdvancements> advancements;
    std::unique_ptr<ServerControl> control;
    std::unique_ptr<ServerDatapacks> datapacks;
    std::unique_ptr<ServerMaps> maps;
    std::unique_ptr<ServerLoot> loot;
    std::unique_ptr<ServerSelectors> selectors;
    std::unique_ptr<ServerStructures> structures;
};

} // namespace basalt::server::runtime::plugins
