#include "WasmWorlds.hpp"

#include <algorithm>
#include <cstdlib>

namespace basalt::server::plugins::wasm::worlds {
namespace {
constexpr std::int32_t kBatch = 16384;
constexpr std::int64_t kMaxFill = 16777216;
constexpr std::int32_t kMaxSections = 64;

constexpr std::int32_t kLoad = 1;
constexpr std::int32_t kNotify = 2;
constexpr std::int32_t kResend = 4;
constexpr std::int32_t kFluid = 512;
constexpr std::int32_t kIndirect = 1024;

[[nodiscard]] bool has(std::int32_t flags, std::int32_t bit) { return (flags & bit) != 0; }

[[nodiscard]] bool validState(State& state, std::int32_t id) {
    return id >= 0 && !state.host->services().worlds().stateText(id).empty();
}

void blockGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::int32_t id = world.stateAt(c.i32(1), c.i32(2), c.i32(3), has(c.i32(4), kLoad));
        if (id < 0) {
            return abi::kErrAbsent;
        }
        return c.memory().put(c.i32(5), c.i32(6), state.host->services().worlds().stateText(id));
    });
}

void blockSet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::optional<std::string_view> text = c.memory().string(c.i32(4), c.i32(5));
        const std::int32_t id = text ? state.host->services().worlds().stateId(*text) : -1;
        if (id < 0) {
            return abi::kErrInvalid;
        }
        return world.setStateAt(c.i32(1), c.i32(2), c.i32(3), id, has(c.i32(6), kNotify)) ? abi::kOk : abi::kErrAbsent;
    });
}

void stateGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t id = world.stateAt(c.i32(1), c.i32(2), c.i32(3), has(c.i32(4), kLoad));
        return id < 0 ? abi::kErrAbsent : id;
    });
}

void stateSet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        if (!validState(state, c.i32(4))) {
            return abi::kErrInvalid;
        }
        return world.setStateAt(c.i32(1), c.i32(2), c.i32(3), c.i32(4), has(c.i32(5), kNotify)) ? abi::kOk
                                                                                                : abi::kErrAbsent;
    });
}

void statesRead(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(2);
        const std::optional<std::vector<std::int32_t>> positions = readInts(c.memory(), c.i32(1), triple(count));
        if (!positions || count < 0) {
            return abi::kErrInvalid;
        }
        std::vector<std::int32_t> states(static_cast<std::size_t>(count), -1);
        static_cast<void>(world.readStates(*positions, states, has(c.i32(4), kLoad)));
        const std::int32_t written = writeInts(c.memory(), c.i32(3), states);
        return written < 0 ? written : count;
    });
}

void statesWrite(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(3);
        const std::optional<std::vector<std::int32_t>> positions = readInts(c.memory(), c.i32(1), triple(count));
        const std::optional<std::vector<std::int32_t>> states = readInts(c.memory(), c.i32(2), count);
        if (!positions || !states || count < 0) {
            return abi::kErrInvalid;
        }
        return world.writeStates(*positions, *states, has(c.i32(4), kNotify));
    });
}

void fill(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::array<std::int32_t, 3> low{std::min(c.i32(1), c.i32(4)), std::min(c.i32(2), c.i32(5)),
                                              std::min(c.i32(3), c.i32(6))};
        const std::array<std::int32_t, 3> high{std::max(c.i32(1), c.i32(4)), std::max(c.i32(2), c.i32(5)),
                                               std::max(c.i32(3), c.i32(6))};
        const std::int64_t volume = (static_cast<std::int64_t>(high[0]) - low[0] + 1) *
                                    (static_cast<std::int64_t>(high[1]) - low[1] + 1) *
                                    (static_cast<std::int64_t>(high[2]) - low[2] + 1);
        const std::int32_t flags = c.i32(8);
        if (volume > kMaxFill || !validState(state, c.i32(7))) {
            return abi::kErrInvalid;
        }
        if (has(flags, kLoad)) {
            for (std::int32_t cz = low[2] >> 4; cz <= high[2] >> 4; ++cz) {
                for (std::int32_t cx = low[0] >> 4; cx <= high[0] >> 4; ++cx) {
                    static_cast<void>(world.loadChunk(cx, cz, false));
                }
            }
        }
        std::vector<std::int32_t> positions;
        std::vector<std::int32_t> states;
        positions.reserve(static_cast<std::size_t>(kBatch) * 3);
        std::int32_t changed = 0;
        const auto flush = [&] {
            states.assign(positions.size() / 3, c.i32(7));
            changed += world.writeStates(positions, states, has(flags, kNotify));
            positions.clear();
        };
        for (std::int32_t y = low[1]; y <= high[1]; ++y) {
            for (std::int32_t z = low[2]; z <= high[2]; ++z) {
                for (std::int32_t x = low[0]; x <= high[0]; ++x) {
                    positions.insert(positions.end(), {x, y, z});
                    if (positions.size() >= static_cast<std::size_t>(kBatch) * 3) {
                        flush();
                    }
                }
            }
        }
        if (!positions.empty()) {
            flush();
        }
        return changed;
    });
}

void sectionRead(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        std::vector<std::int32_t> states(kSectionBlocks);
        if (!world.readSection(c.i32(1), c.i32(2), c.i32(3), has(c.i32(4), kLoad), states)) {
            return abi::kErrAbsent;
        }
        return writeInts(c.memory(), c.i32(5), states);
    });
}

void sectionWrite(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::vector<std::int32_t>> states = readInts(c.memory(), c.i32(4), kSectionBlocks);
        if (!states) {
            return abi::kErrInvalid;
        }
        if (!world.chunkLoaded(c.i32(1), c.i32(3))) {
            return abi::kErrAbsent;
        }
        return world.writeSection(c.i32(1), c.i32(2), c.i32(3), *states, has(c.i32(5), kNotify)) ? abi::kOk
                                                                                                 : abi::kErrAbsent;
    });
}

void chunkRead(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const bool load = has(c.i32(3), kLoad);
        if (!load && !world.chunkLoaded(c.i32(1), c.i32(2))) {
            return abi::kErrAbsent;
        }
        const std::int32_t needed = (std::max(world.height(), 0) >> 4) * kSectionBlocks;
        if (needed > c.i32(5)) {
            return needed;
        }
        std::vector<std::int32_t> states(static_cast<std::size_t>(needed));
        if (!world.readSections(c.i32(1), c.i32(2), load, states)) {
            return abi::kErrAbsent;
        }
        return putInts(c.memory(), c.i32(4), c.i32(5), states);
    });
}

void chunkApply(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(4);
        if (count < 0 || count > kMaxSections) {
            return abi::kErrInvalid;
        }
        const std::optional<std::vector<std::int32_t>> ys = readInts(c.memory(), c.i32(3), count);
        const std::optional<std::vector<std::int32_t>> states = readInts(c.memory(), c.i32(5), count * kSectionBlocks);
        if (!ys || !states) {
            return abi::kErrInvalid;
        }
        if (!world.chunkLoaded(c.i32(1), c.i32(2))) {
            return abi::kErrAbsent;
        }
        std::vector<std::int32_t> previous(c.i32(7) != 0 ? states->size() : 0);
        const std::int32_t changed =
            world.applySections(c.i32(1), c.i32(2), *ys, *states, has(c.i32(6), kResend), previous);
        if (changed < 0) {
            return abi::kErrInvalid;
        }
        if (!previous.empty() && writeInts(c.memory(), c.i32(7), previous) < 0) {
            return abi::kErrInvalid;
        }
        return changed;
    });
}

void changesApply(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(3);
        const std::optional<std::vector<std::int32_t>> positions = readInts(c.memory(), c.i32(1), triple(count));
        const std::optional<std::vector<std::int32_t>> states = readInts(c.memory(), c.i32(2), count);
        if (!positions || !states || count < 0) {
            return abi::kErrInvalid;
        }
        return world.applyChanges(*positions, *states, static_cast<std::uint32_t>(c.i32(4)));
    });
}

void blocksSettle(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(3);
        const std::optional<std::vector<std::int32_t>> positions = readInts(c.memory(), c.i32(1), triple(count));
        std::optional<std::vector<std::int32_t>> previous = std::vector<std::int32_t>{};
        if (c.i32(2) != 0) {
            previous = readInts(c.memory(), c.i32(2), count);
        }
        if (!positions || !previous || count < 0) {
            return abi::kErrInvalid;
        }
        return world.settleBlocks(*positions, *previous, static_cast<std::uint32_t>(c.i32(4)));
    });
}

void blocksNotify(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(3);
        const std::optional<std::vector<std::int32_t>> positions = readInts(c.memory(), c.i32(1), triple(count));
        std::optional<std::vector<std::int32_t>> sources = std::vector<std::int32_t>{};
        if (c.i32(2) != 0) {
            sources = readInts(c.memory(), c.i32(2), triple(count));
        }
        if (!positions || !sources || count < 0) {
            return abi::kErrInvalid;
        }
        return world.neighborChanged(*positions, *sources);
    });
}

void blocksReshape(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::int32_t count = c.i32(2);
        const std::optional<std::vector<std::int32_t>> positions = readInts(c.memory(), c.i32(1), triple(count));
        if (!positions || count < 0) {
            return abi::kErrInvalid;
        }
        return world.updateShapes(*positions, {}, has(c.i32(3), kIndirect));
    });
}

void stateValidAt(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        if (world.stateAt(c.i32(1), c.i32(2), c.i32(3), false) < 0) {
            return abi::kErrAbsent;
        }
        return world.validState(c.i32(1), c.i32(2), c.i32(3), c.i32(4));
    });
}

void tickSchedule(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        return world.scheduleTick(c.i32(1), c.i32(2), c.i32(3), c.i32(4), has(c.i32(5), kFluid)) ? 1 : 0;
    });
}

void tickPending(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        return world.hasScheduledTick(c.i32(1), c.i32(2), c.i32(3)) ? 1 : 0;
    });
}

void blockEntityGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::vector<std::uint8_t>> nbt = world.blockEntityNbt(c.i32(1), c.i32(2), c.i32(3));
        return nbt ? putBytes(c.memory(), c.i32(4), c.i32(5), *nbt) : abi::kErrAbsent;
    });
}

void blockEntityEdit(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::vector<std::uint8_t>> nbt = readBytes(c.memory(), c.i32(5), c.i32(6));
        if (!nbt) {
            return abi::kErrInvalid;
        }
        bool done = false;
        switch (c.i32(1)) {
        case 0:
            done = world.setBlockEntityNbt(c.i32(2), c.i32(3), c.i32(4), *nbt);
            break;
        case 1:
            done = world.createBlockEntity(c.i32(2), c.i32(3), c.i32(4), *nbt);
            break;
        case 2:
            done = world.removeBlockEntity(c.i32(2), c.i32(3), c.i32(4));
            break;
        default:
            return abi::kErrInvalid;
        }
        return done ? abi::kOk : abi::kErrAbsent;
    });
}

void blockEntities(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        if (!world.chunkLoaded(c.i32(1), c.i32(2))) {
            return abi::kErrAbsent;
        }
        return putInts(c.memory(), c.i32(3), c.i32(4), world.blockEntityPositions(c.i32(1), c.i32(2)));
    });
}

void dataGet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        std::optional<std::vector<std::uint8_t>> nbt;
        switch (c.i32(1)) {
        case 0:
            nbt = world.pluginData();
            break;
        case 1:
            nbt = world.chunkPluginData(c.i32(2), c.i32(4));
            break;
        case 2:
            nbt = world.blockEntityPluginData(c.i32(2), c.i32(3), c.i32(4));
            break;
        default:
            return abi::kErrInvalid;
        }
        return nbt ? putBytes(c.memory(), c.i32(5), c.i32(6), *nbt) : abi::kErrAbsent;
    });
}

void dataSet(void* user, abi::Call& c) {
    withWorld(user, c, [&](State&, PluginWorldRef& world) {
        const std::optional<std::vector<std::uint8_t>> nbt = readBytes(c.memory(), c.i32(5), c.i32(6));
        if (!nbt) {
            return abi::kErrInvalid;
        }
        bool done = false;
        switch (c.i32(1)) {
        case 0:
            done = world.setPluginData(*nbt);
            break;
        case 1:
            done = world.setChunkPluginData(c.i32(2), c.i32(4), *nbt);
            break;
        case 2:
            done = world.setBlockEntityPluginData(c.i32(2), c.i32(3), c.i32(4), *nbt);
            break;
        default:
            return abi::kErrInvalid;
        }
        return done ? abi::kOk : abi::kErrAbsent;
    });
}
} // namespace
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsBlocksImports(State& state, std::vector<abi::HostImport>& out) {
    using namespace worlds;
    void* user = &state;
    out.insert(out.end(), {
                              {"world_block_get", blockGet, user},
                              {"world_block_set", blockSet, user},
                              {"world_state_get", stateGet, user},
                              {"world_state_set", stateSet, user},
                              {"world_states_read", statesRead, user},
                              {"world_states_write", statesWrite, user},
                              {"world_fill", fill, user},
                              {"world_section_read", sectionRead, user},
                              {"world_section_write", sectionWrite, user},
                              {"world_chunk_read", chunkRead, user},
                              {"world_chunk_apply", chunkApply, user},
                              {"world_changes_apply", changesApply, user},
                              {"world_blocks_settle", blocksSettle, user},
                              {"world_blocks_notify", blocksNotify, user},
                              {"world_blocks_reshape", blocksReshape, user},
                              {"world_state_valid", stateValidAt, user},
                              {"world_tick_schedule", tickSchedule, user},
                              {"world_tick_pending", tickPending, user},
                              {"world_block_entity_get", blockEntityGet, user},
                              {"world_block_entity_edit", blockEntityEdit, user},
                              {"world_block_entities", blockEntities, user},
                              {"world_data_get", dataGet, user},
                              {"world_data_set", dataSet, user},
                          });
}
} // namespace basalt::server::plugins::wasm
