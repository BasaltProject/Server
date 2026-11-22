#include "WasmWorlds.hpp"

#include <algorithm>
#include <functional>
#include <memory>

namespace basalt::server::plugins::wasm::worlds {
namespace {
constexpr std::int32_t kLoad = 1;
constexpr std::int32_t kGenerate = 8;

[[nodiscard]] bool has(std::int32_t flags, std::int32_t bit) { return (flags & bit) != 0; }

[[nodiscard]] std::string holderTag(State& state, std::string_view tag) {
    return state.plugin->manifest.id + ":" + std::string{tag};
}

/// A service completion that may fire inside the call that asked for it; those are held back to the next turn.
struct Latch {
    bool inCall = true;
    std::optional<Value> early;
};

[[nodiscard]] std::function<void(Value)> completion(State& state, std::int32_t request,
                                                    const std::shared_ptr<Latch>& latch) {
    const std::shared_ptr<State*> link = state.link;
    return [link, request, latch](Value value) {
        State* alive = *link;
        if (alive == nullptr) {
            return;
        }
        if (latch->inCall) {
            latch->early = std::move(value);
            return;
        }
        completeRequest(*alive, request, value);
    };
}

void settle(State& state, std::int32_t request, const std::shared_ptr<Latch>& latch) {
    latch->inCall = false;
    if (latch->early) {
        std::shared_ptr<Latch> keep = latch;
        const std::shared_ptr<State*> link = state.link;
        static_cast<void>(state.host->services().tasks().after(0, [link, request, keep] {
            if (State* alive = *link; alive != nullptr) {
                completeRequest(*alive, request, *keep->early);
            }
        }));
    }
}

void chunkStatus(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) { return world.chunkStatus(c.i32(1), c.i32(2)); });
}

void chunkLoad(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        return world.loadChunk(c.i32(1), c.i32(2), has(c.i32(3), kGenerate)) ? 1 : 0;
    });
}

void chunkUnload(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) { return world.unloadChunk(c.i32(1), c.i32(2)) ? 1 : 0; });
}

void chunkRequest(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::int32_t request = beginRequest(state, c.i32(5));
        if (request == 0) {
            return abi::kErrInvalid;
        }
        const auto latch = std::make_shared<Latch>();
        const std::function<void(Value)> finish = completion(state, request, latch);
        world.requestChunk(c.i32(1), c.i32(2), has(c.i32(3), kGenerate), c.i32(4),
                           [finish](bool loaded) { finish(Value{loaded}); });
        settle(state, request, latch);
        return request;
    });
}

void chunksLoaded(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        return putInts(c.memory(), c.i32(1), c.i32(2), world.loadedChunks());
    });
}

void chunkHold(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::optional<std::string_view> tag = c.memory().string(c.i32(1), c.i32(2));
        const std::int32_t count = c.i32(4);
        const std::optional<std::vector<std::int32_t>> chunks = readInts(c.memory(), c.i32(3), pairs(count));
        if (!tag || !chunks) {
            return abi::kErrInvalid;
        }
        return world.holdChunks(holderTag(state, *tag), *chunks) ? abi::kOk : abi::kErrAbsent;
    });
}

void chunkRelease(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::optional<std::string_view> tag = c.memory().string(c.i32(1), c.i32(2));
        const std::int32_t count = c.i32(4);
        if (!tag) {
            return abi::kErrInvalid;
        }
        if (count < 0) {
            world.releaseChunks(holderTag(state, *tag));
            return abi::kOk;
        }
        const std::optional<std::vector<std::int32_t>> chunks = readInts(c.memory(), c.i32(3), pairs(count));
        if (!chunks) {
            return abi::kErrInvalid;
        }
        world.releaseChunkPairs(holderTag(state, *tag), *chunks);
        return abi::kOk;
    });
}

void chunkHeld(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::optional<std::string_view> tag = c.memory().string(c.i32(1), c.i32(2));
        if (!tag) {
            return abi::kErrInvalid;
        }
        return putInts(c.memory(), c.i32(3), c.i32(4), world.heldChunks(holderTag(state, *tag)));
    });
}

void chunksResend(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(2);
        const std::optional<std::vector<std::int32_t>> chunks = readInts(c.memory(), c.i32(1), pairs(count));
        if (!chunks) {
            return abi::kErrInvalid;
        }
        world.resendChunks(*chunks);
        return abi::kOk;
    });
}

void storedGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::vector<std::uint8_t>> nbt = world.storedChunkNbt(c.i32(1), c.i32(2));
        return nbt ? putBytes(c.memory(), c.i32(3), c.i32(4), *nbt) : abi::kErrAbsent;
    });
}

void storedPut(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        if (!state.capable("worldgen")) {
            return abi::kErrDenied;
        }
        const std::optional<std::vector<std::uint8_t>> nbt = readBytes(c.memory(), c.i32(3), c.i32(4));
        if (!nbt) {
            return abi::kErrInvalid;
        }
        return world.storeChunkNbt(c.i32(1), c.i32(2), *nbt) ? abi::kOk : abi::kErrAbsent;
    });
}

void storedList(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        return putInts(c.memory(), c.i32(3), c.i32(4), world.storedChunksIn(c.i32(1), c.i32(2)));
    });
}

void lightAt(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t light = world.lightAt(c.i32(1), c.i32(2), c.i32(3), c.i32(4));
        return light < 0 ? abi::kErrAbsent : light;
    });
}

void lightRead(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        std::vector<std::uint8_t> nibbles(kLightBytes);
        if (!world.readLight(c.i32(1), c.i32(2), c.i32(3), c.i32(4), has(c.i32(5), kLoad), nibbles)) {
            return abi::kErrAbsent;
        }
        return putBytes(c.memory(), c.i32(6), c.i32(7), nibbles);
    });
}

void lightWrite(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::vector<std::uint8_t>> nibbles = readBytes(c.memory(), c.i32(5), c.i32(6));
        if (!nibbles || nibbles->size() != static_cast<std::size_t>(kLightBytes)) {
            return abi::kErrInvalid;
        }
        return world.writeLight(c.i32(1), c.i32(2), c.i32(3), c.i32(4), *nibbles) ? abi::kOk : abi::kErrAbsent;
    });
}

void lightCheck(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::int32_t count = c.i32(2);
        const std::optional<std::vector<std::int32_t>> chunks = readInts(c.memory(), c.i32(1), pairs(count));
        if (!chunks) {
            return abi::kErrInvalid;
        }
        if (c.i32(3) == 0) {
            world.relightChunks(*chunks);
            return abi::kOk;
        }
        const std::int32_t request = beginRequest(state, c.i32(3));
        if (request == 0) {
            return abi::kErrInvalid;
        }
        const auto latch = std::make_shared<Latch>();
        const std::function<void(Value)> finish = completion(state, request, latch);
        world.lightJob(*chunks, [finish](std::int32_t loaded) { finish(Value{static_cast<std::int64_t>(loaded)}); });
        settle(state, request, latch);
        return request;
    });
}

void biomeGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string> biome = world.biomeAt(c.i32(1), c.i32(2), c.i32(3));
        return biome ? c.memory().put(c.i32(4), c.i32(5), *biome) : abi::kErrAbsent;
    });
}

void biomeSet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::string_view> biome = c.memory().string(c.i32(4), c.i32(5));
        if (!biome) {
            return abi::kErrInvalid;
        }
        return world.setBiomeAt(c.i32(1), c.i32(2), c.i32(3), *biome) ? abi::kOk : abi::kErrAbsent;
    });
}

void biomesRead(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        if (!world.chunkLoaded(c.i32(1), c.i32(2))) {
            return abi::kErrAbsent;
        }
        const std::int32_t needed = (std::max(world.height(), 0) >> 2) * 16;
        if (needed > c.i32(4)) {
            return needed;
        }
        std::vector<std::int32_t> ids(static_cast<std::size_t>(needed));
        if (!world.readBiomes(c.i32(1), c.i32(2), ids)) {
            return abi::kErrAbsent;
        }
        return putInts(c.memory(), c.i32(3), c.i32(4), ids);
    });
}

void biomesWrite(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::vector<std::int32_t>> ids = readInts(c.memory(), c.i32(3), c.i32(4));
        if (!ids) {
            return abi::kErrInvalid;
        }
        return world.writeBiomes(c.i32(1), c.i32(2), *ids) ? abi::kOk : abi::kErrAbsent;
    });
}
} // namespace
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsChunksImports(State& state, std::vector<abi::HostImport>& out) {
    using namespace worlds;
    void* user = &state;
    out.insert(out.end(), {
                              {"world_chunk_status", chunkStatus, user},   {"world_chunk_load", chunkLoad, user},
                              {"world_chunk_unload", chunkUnload, user},   {"world_chunk_request", chunkRequest, user},
                              {"world_chunks_loaded", chunksLoaded, user}, {"world_chunk_hold", chunkHold, user},
                              {"world_chunk_release", chunkRelease, user}, {"world_chunk_held", chunkHeld, user},
                              {"world_chunks_resend", chunksResend, user}, {"world_stored_get", storedGet, user},
                              {"world_stored_put", storedPut, user},       {"world_stored_list", storedList, user},
                              {"world_light_at", lightAt, user},           {"world_light_read", lightRead, user},
                              {"world_light_write", lightWrite, user},     {"world_light_check", lightCheck, user},
                              {"world_biome_get", biomeGet, user},         {"world_biome_set", biomeSet, user},
                              {"world_biomes_read", biomesRead, user},     {"world_biomes_write", biomesWrite, user},
                          });
}
} // namespace basalt::server::plugins::wasm
