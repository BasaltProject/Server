#pragma once

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/sched/OnOwner.hpp>
#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/SavedDataFiles.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/runtime/world/WorldSpawnPoint.hpp>
#include <basalt/server/world/LeafDistances.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/SimpleWorldGenerators.hpp>
#include <basalt/server/world/climate/Sampler.hpp>
#include <basalt/server/world/climate/SpawnFinder.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <set>

namespace basalt::server::runtime::world::loadedWorld {

namespace terrain = server::world::terrain;

using server::world::Chunk;

[[nodiscard]] inline std::string levelKeyOf(const config::WorldDeclaration& declaration, bool vanillaKey) {
    if (vanillaKey || declaration.isDefault) {
        return config::dimensionRegistryKey(declaration.dimension);
    }
    std::string key = "basalt:";
    for (const char letter : declaration.name) {
        const bool plain =
            (std::isalnum(static_cast<unsigned char>(letter)) != 0) || letter == '.' || letter == '_' || letter == '-';
        key.push_back(plain ? static_cast<char>(std::tolower(static_cast<unsigned char>(letter))) : '_');
    }
    return key;
}

[[nodiscard]] inline std::int64_t epochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace basalt::server::runtime::world::loadedWorld
