#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <map>
#include <unordered_map>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

constexpr std::size_t kSectionBlocks = 4096;

[[nodiscard]] std::int32_t intAt(LuaArgs& lua, std::int32_t index) {
    return static_cast<std::int32_t>(lua.checkInteger(index));
}

[[nodiscard]] std::vector<std::int32_t> statesFrom(LuaArgs& lua, std::int32_t index, std::size_t expected) {
    if (!lua.isTable(index) || static_cast<std::size_t>(lua.length(index)) != expected) {
        lua.raise("expected a list of " + std::to_string(expected) + " numbers");
    }
    std::vector<std::int32_t> out(expected);
    for (std::size_t position = 0; position < expected; position++) {
        lua.pushIndex(index, static_cast<std::int64_t>(position) + 1);
        out[position] = lua.isNumber(-1) ? static_cast<std::int32_t>(lua.toInteger(-1)) : -1;
        lua.pop(1);
    }
    return out;
}

void pushInts(LuaArgs& lua, std::span<const std::int32_t> values) {
    lua.pushTable();
    for (std::size_t position = 0; position < values.size(); position++) {
        lua.pushInteger(values[position]);
        lua.setIndex(static_cast<std::int64_t>(position) + 1);
    }
}

/// A list of `{ chunk_x, chunk_z }` pairs, flattened to x, z, x, z...
[[nodiscard]] std::vector<std::int32_t> chunkPairs(LuaArgs& lua, std::int32_t index) {
    if (!lua.isTable(index)) {
        lua.raise("expected a list of { chunk_x, chunk_z } pairs");
    }
    std::vector<std::int32_t> out;
    const std::int64_t length = lua.length(index);
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        if (!lua.isTable(-1)) {
            lua.raise("expected a list of { chunk_x, chunk_z } pairs");
        }
        for (const std::int64_t part : {1, 2}) {
            lua.pushIndex(lua.top(), part);
            out.push_back(static_cast<std::int32_t>(lua.toInteger(-1)));
            lua.pop(1);
        }
        lua.pop(1);
    }
    return out;
}

[[nodiscard]] bool flag(LuaArgs& lua, std::int32_t options, std::string_view key, bool fallback) {
    if (!lua.isTable(options)) {
        return fallback;
    }
    lua.pushField(options, key);
    const bool value = lua.isNil(-1) ? fallback : lua.toBoolean(-1);
    lua.pop(1);
    return value;
}

[[nodiscard]] std::size_t biomeCells(const PluginWorldRef& world) {
    return static_cast<std::size_t>(std::max(world.height(), 0) / 4) * 16;
}

struct Sections {
    std::vector<std::int32_t> ys;
    std::vector<std::int32_t> states;
};

/// A table of section_y to a list of 4096 state ids, taken in ascending section order.
[[nodiscard]] Sections sectionsFrom(LuaArgs& lua, std::int32_t index) {
    if (!lua.isTable(index)) {
        lua.raise("sections is a table of section_y to a list of 4096 state ids");
    }
    std::map<std::int32_t, std::vector<std::int32_t>> ordered;
    lua.pushNil();
    while (lua.nextEntry(index)) {
        ordered[static_cast<std::int32_t>(lua.toInteger(-2))] = statesFrom(lua, lua.top(), kSectionBlocks);
        lua.pop(1);
    }
    Sections out;
    for (auto& [y, states] : ordered) {
        out.ys.push_back(y);
        out.states.insert(out.states.end(), states.begin(), states.end());
    }
    return out;
}

void pushPrevious(LuaArgs& lua, std::span<const std::int32_t> ys, std::span<const std::int32_t> previous) {
    lua.pushTable();
    for (std::size_t section = 0; section < ys.size(); section++) {
        pushInts(lua, previous.subspan(section * kSectionBlocks, kSectionBlocks));
        lua.setIndex(ys[section]);
    }
}

[[nodiscard]] std::optional<std::string> snbtOf(const std::optional<std::vector<std::uint8_t>>& bytes) {
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    if (!read.has_value()) {
        return std::nullopt;
    }
    return wire::nbt::snbt::stringify(read->tag);
}

[[nodiscard]] std::vector<std::uint8_t> bytesOf(LuaArgs& lua, std::int32_t index) {
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(lua.checkString(index));
    if (!parsed.has_value()) {
        lua.raise("that is not valid NBT text");
    }
    return wire::nbt::nbtIO::write(*parsed);
}

void pushText(LuaArgs& lua, const std::optional<std::string>& text) {
    if (text.has_value()) {
        lua.pushString(*text);
    } else {
        lua.pushNil();
    }
}

struct Spot {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t next = 0;
};

[[nodiscard]] Spot spotAt(LuaArgs& lua, std::int32_t base) {
    const std::optional<PluginPoint> point = pointFrom(lua, base);
    if (point.has_value()) {
        return {static_cast<std::int32_t>(std::floor(point->x)), static_cast<std::int32_t>(std::floor(point->y)),
                static_cast<std::int32_t>(std::floor(point->z)), base + 1};
    }
    return {intAt(lua, base), intAt(lua, base + 1), intAt(lua, base + 2), base + 3};
}

/// A list of { x, y, z } triples, flattened to x, y, z, x, y, z...
[[nodiscard]] std::vector<std::int32_t> blockTriples(LuaArgs& lua, std::int32_t index) {
    if (!lua.isTable(index)) {
        lua.raise("expected a list of { x, y, z } positions");
    }
    std::vector<std::int32_t> out;
    const std::int64_t length = lua.length(index);
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        if (!lua.isTable(-1)) {
            lua.raise("expected a list of { x, y, z } positions");
        }
        for (const std::int64_t part : {1, 2, 3}) {
            lua.pushIndex(lua.top(), part);
            out.push_back(static_cast<std::int32_t>(lua.toInteger(-1)));
            lua.pop(1);
        }
        lua.pop(1);
    }
    return out;
}

/// A list of state ids, one per position (shorter lists leave the rest unknown, as -1).
[[nodiscard]] std::vector<std::int32_t> stateList(LuaArgs& lua, std::int32_t index, std::size_t count) {
    std::vector<std::int32_t> out(count, -1);
    const std::int64_t length = std::min<std::int64_t>(lua.length(index), static_cast<std::int64_t>(count));
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        out[static_cast<std::size_t>(position - 1)] =
            lua.isNumber(-1) ? static_cast<std::int32_t>(lua.toInteger(-1)) : -1;
        lua.pop(1);
    }
    return out;
}

/// A block face by name: down, up, north, south, west or east.
[[nodiscard]] std::int32_t faceFrom(LuaArgs& lua, std::int32_t index) {
    const std::string name = lua.isString(index) ? lua.toText(index) : std::string{};
    for (std::int32_t face = 0; face < 6; face++) {
        if (name == std::array<std::string_view, 6>{"down", "up", "north", "south", "west",
                                                    "east"}[static_cast<std::size_t>(face)]) {
            return face;
        }
    }
    lua.raise("a face is down, up, north, south, west or east");
    return 1;
}

/// A light layer as "block" or "sky", or as 0 or 1.
[[nodiscard]] std::int32_t layerFrom(LuaArgs& lua, std::int32_t index) {
    if (lua.isNumber(index)) {
        return lua.toInteger(index) == 1 ? 1 : 0;
    }
    const std::string name = lua.isString(index) ? lua.toText(index) : std::string{};
    if (name == "sky") {
        return 1;
    }
    if (name != "block") {
        lua.raise("a light layer is 'block' or 'sky'");
    }
    return 0;
}

/// Effects as a number, or a list of names.
[[nodiscard]] std::uint32_t effectsFrom(LuaArgs& lua, std::int32_t index) {
    if (lua.isNumber(index)) {
        return static_cast<std::uint32_t>(lua.toInteger(index));
    }
    if (!lua.isTable(index)) {
        return PluginWorldRef::BlockEffects::kNeighbors | PluginWorldRef::BlockEffects::kShapes |
               PluginWorldRef::BlockEffects::kClients;
    }
    std::uint32_t mask = 0;
    const std::int64_t length = lua.length(index);
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        const std::string name = lua.toText(-1);
        lua.pop(1);
        using Effects = PluginWorldRef::BlockEffects;
        if (name == "neighbors") {
            mask |= Effects::kNeighbors;
        } else if (name == "shapes") {
            mask |= Effects::kShapes;
        } else if (name == "light") {
            mask |= Effects::kLight;
        } else if (name == "entities") {
            mask |= Effects::kEntities;
        } else if (name == "poi") {
            mask |= Effects::kPoi;
        } else if (name == "ticks") {
            mask |= Effects::kTicks;
        } else if (name == "clients") {
            mask |= Effects::kClients;
        } else if (name == "place") {
            mask |= Effects::kPlace;
        } else if (name == "departure") {
            mask |= Effects::kDeparture;
        } else {
            lua.raise("'" + name +
                      "' is not an effect (neighbors, shapes, light, entities, poi, ticks, clients, place, departure)");
        }
    }
    return mask;
}

constexpr std::size_t kLightSectionBytes = 2048;

[[nodiscard]] std::string_view statusName(std::int32_t status) {
    switch (status) {
    case 1:
        return "queued";
    case 2:
        return "loaded";
    case 3:
        return "ticking";
    default:
        return "unloaded";
    }
}
} // namespace

void addBulkWorldMethods(PluginObject& object, const std::shared_ptr<PluginWorldRef>& world,
                         const std::shared_ptr<WorldHooks>& hooks) {
    object.reads("block_version", [world] { return PluginValue{static_cast<std::int64_t>(world->blockVersion())}; })
        .reads("folder", [world] { return PluginValue{world->folder()}; })
        .method("section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::vector<std::int32_t> out(kSectionBlocks);
                    if (!world->readSection(intAt(args, base), intAt(args, base + 1), intAt(args, base + 2),
                                            args.isNil(base + 3) || args.booleanOr(base + 3, true), out)) {
                        args.pushNil();
                        return 1;
                    }
                    pushInts(args, out);
                    return 1;
                })
        .method("set_section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> states = statesFrom(args, base + 3, kSectionBlocks);
                    args.pushBoolean(world->writeSection(intAt(args, base), intAt(args, base + 1),
                                                         intAt(args, base + 2), states,
                                                         args.isNil(base + 4) || args.booleanOr(base + 4, true)));
                    return 1;
                })
        .method("apply_chunk",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const Sections sections = sectionsFrom(args, base + 2);
                    const bool resend = flag(args, base + 3, "resend", true);
                    const bool wantPrevious = flag(args, base + 3, "previous", false);
                    std::vector<std::int32_t> previous(wantPrevious ? sections.states.size() : 0);
                    const std::int32_t changed = world->applySections(intAt(args, base), intAt(args, base + 1),
                                                                      sections.ys, sections.states, resend, previous);
                    if (changed < 0) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushInteger(changed);
                    if (!wantPrevious) {
                        return 1;
                    }
                    pushPrevious(args, sections.ys, previous);
                    return 2;
                })
        .method("apply_chunks",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (!args.isTable(base)) {
                        args.raise("apply_chunks takes a list of { x, z, sections }");
                    }
                    std::vector<std::int32_t> chunks;
                    Sections all;
                    const std::int64_t length = args.length(base);
                    for (std::int64_t position = 1; position <= length; position++) {
                        args.pushIndex(base, position);
                        const std::int32_t entry = args.top();
                        args.pushField(entry, "x");
                        const std::int32_t chunkX = intAt(args, -1);
                        args.pop(1);
                        args.pushField(entry, "z");
                        const std::int32_t chunkZ = intAt(args, -1);
                        args.pop(1);
                        args.pushField(entry, "sections");
                        const Sections sections = sectionsFrom(args, args.top());
                        args.pop(2);
                        chunks.insert(chunks.end(), {chunkX, chunkZ, static_cast<std::int32_t>(sections.ys.size())});
                        all.ys.insert(all.ys.end(), sections.ys.begin(), sections.ys.end());
                        all.states.insert(all.states.end(), sections.states.begin(), sections.states.end());
                    }
                    const bool resend = flag(args, base + 1, "resend", true);
                    const bool wantPrevious = flag(args, base + 1, "previous", false);
                    std::vector<std::int32_t> previous(wantPrevious ? all.states.size() : 0);
                    const std::int32_t changed = world->applyChunks(chunks, all.ys, all.states, resend, previous);
                    if (changed < 0) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushInteger(changed);
                    if (!wantPrevious) {
                        return 1;
                    }
                    args.pushTable();
                    std::size_t section = 0;
                    for (std::size_t chunk = 0; chunk < chunks.size() / 3; chunk++) {
                        const auto count = static_cast<std::size_t>(chunks[(chunk * 3) + 2]);
                        pushPrevious(args, std::span<const std::int32_t>{all.ys}.subspan(section, count),
                                     std::span<const std::int32_t>{previous}.subspan(section * kSectionBlocks,
                                                                                     count * kSectionBlocks));
                        args.setIndex(static_cast<std::int64_t>(chunk) + 1);
                        section += count;
                    }
                    return 2;
                })
        .method("chunk_sections",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::size_t count = static_cast<std::size_t>(std::max(world->height(), 0)) >> 4;
                    std::vector<std::int32_t> all(count * kSectionBlocks);
                    if (!world->readSections(intAt(args, base), intAt(args, base + 1),
                                             args.isNil(base + 2) || args.booleanOr(base + 2, true), all)) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushTable();
                    const std::int32_t first = world->minHeight() >> 4;
                    for (std::size_t section = 0; section < count; section++) {
                        pushInts(args,
                                 std::span<const std::int32_t>{all}.subspan(section * kSectionBlocks, kSectionBlocks));
                        args.setIndex(first + static_cast<std::int64_t>(section));
                    }
                    return 1;
                })
        .method("chunk_status",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushString(
                        std::string{statusName(world->chunkStatus(intAt(args, base), intAt(args, base + 1)))});
                    return 1;
                })
        .method("hold_chunks",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->holdChunks(args.checkString(base), chunkPairs(args, base + 1)));
                    return 1;
                })
        .method("release_chunks",
                [world](LuaArgs& args) {
                    world->releaseChunks(args.checkString(firstArgument(args)));
                    return 0;
                })
        .method("update_neighbors",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    const std::vector<std::int32_t> previous =
                        args.isTable(base + 2) ? stateList(args, base + 2, at.size() / 3) : std::vector<std::int32_t>{};
                    args.pushInteger(world->settleBlocks(at, previous, effectsFrom(args, base + 1)));
                    return 1;
                })
        .method("use_item_on",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    const std::int32_t face = faceFrom(args, spot.next);
                    args.pushInteger(world->useItemOn(
                        spot.x, spot.y, spot.z, face, args.checkString(spot.next + 1),
                        static_cast<float>(args.isNumber(spot.next + 2) ? args.toNumber(spot.next + 2) : 0.0),
                        static_cast<float>(args.isNumber(spot.next + 3) ? args.toNumber(spot.next + 3) : 0.0)));
                    return 1;
                })
        .method("valid_state",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushInteger(world->validState(spot.x, spot.y, spot.z,
                                                       static_cast<std::int32_t>(args.toInteger(spot.next))));
                    return 1;
                })
        .method("schedule_tick",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    const bool fluid = args.isString(spot.next + 1) && args.toText(spot.next + 1) == "fluid";
                    args.pushBoolean(world->scheduleTick(spot.x, spot.y, spot.z,
                                                         static_cast<std::int32_t>(args.toInteger(spot.next)), fluid));
                    return 1;
                })
        .method("neighbor_changed",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    const std::vector<std::int32_t> sources =
                        args.isTable(base + 1) ? stateList(args, base + 1, at.size() / 3) : std::vector<std::int32_t>{};
                    args.pushInteger(world->neighborChanged(at, sources));
                    return 1;
                })
        .method("update_shapes",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    const bool indirect = args.isTable(base + 1);
                    const std::vector<std::int32_t> states =
                        indirect ? stateList(args, base + 1, at.size() / 3) : std::vector<std::int32_t>{};
                    args.pushInteger(world->updateShapes(at, states, indirect));
                    return 1;
                })
        .method("apply_changes",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    std::vector<std::int32_t> states(at.size() / 3);
                    if (!args.isTable(base + 1) || static_cast<std::size_t>(args.length(base + 1)) != states.size()) {
                        args.raise("apply_changes(positions, states, effects) needs one state per position");
                    }
                    for (std::size_t index = 0; index < states.size(); index++) {
                        args.pushIndex(base + 1, static_cast<std::int64_t>(index) + 1);
                        states[index] = static_cast<std::int32_t>(args.toInteger(-1));
                        args.pop(1);
                    }
                    args.pushInteger(world->applyChanges(at, states, effectsFrom(args, base + 2)));
                    return 1;
                })
        .method("read_states",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    std::vector<std::int32_t> out(at.size() / 3, -1);
                    world->readStates(at, out, args.isNil(base + 1) || args.booleanOr(base + 1, true));
                    pushInts(args, out);
                    return 1;
                })
        .method("write_states",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> at = blockTriples(args, base);
                    std::vector<std::int32_t> states(at.size() / 3);
                    if (!args.isTable(base + 1) || static_cast<std::size_t>(args.length(base + 1)) != states.size()) {
                        args.raise("write_states(positions, states, notify) needs one state per position");
                    }
                    for (std::size_t index = 0; index < states.size(); index++) {
                        args.pushIndex(base + 1, static_cast<std::int64_t>(index) + 1);
                        states[index] = static_cast<std::int32_t>(args.toInteger(-1));
                        args.pop(1);
                    }
                    args.pushInteger(
                        world->writeStates(at, states, args.isNil(base + 2) || args.booleanOr(base + 2, true)));
                    return 1;
                })
        .method("resend_chunks",
                [world](LuaArgs& args) {
                    world->resendChunks(chunkPairs(args, firstArgument(args)));
                    return 0;
                })
        .method("light_at",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    const std::int32_t level = world->lightAt(spot.x, spot.y, spot.z, layerFrom(args, spot.next));
                    if (level < 0) {
                        args.pushNil();
                    } else {
                        args.pushInteger(level);
                    }
                    return 1;
                })
        .method("light_section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::int32_t layer = layerFrom(args, base + 3);
                    if (args.isNil(base + 5) || args.booleanOr(base + 5, true)) {
                        static_cast<void>(world->lightAt(intAt(args, base) * 16, intAt(args, base + 1) * 16,
                                                         intAt(args, base + 2) * 16, layer));
                    }
                    std::vector<std::uint8_t> out(kLightSectionBytes);
                    if (!world->readLight(intAt(args, base), intAt(args, base + 1), intAt(args, base + 2), layer,
                                          args.isNil(base + 4) || args.booleanOr(base + 4, true), out)) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushString(std::string_view{reinterpret_cast<const char*>(out.data()), out.size()});
                    return 1;
                })
        .method(
            "set_light_section",
            [world](LuaArgs& args) {
                const std::int32_t base = firstArgument(args);
                const std::string bytes = args.checkString(base + 4);
                args.pushBoolean(world->writeLight(
                    intAt(args, base), intAt(args, base + 1), intAt(args, base + 2), layerFrom(args, base + 3),
                    std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}));
                return 1;
            })
        .method("check_light",
                [world](LuaArgs& args) {
                    world->checkLight(blockTriples(args, firstArgument(args)));
                    return 0;
                })
        .method("relight_chunks",
                [world](LuaArgs& args) {
                    world->relightChunks(chunkPairs(args, firstArgument(args)));
                    return 0;
                })
        .method("loaded_chunks",
                [world](LuaArgs& args) {
                    const std::vector<std::int32_t> flat = world->loadedChunks();
                    args.pushTable();
                    for (std::size_t pair = 0; pair + 1 < flat.size(); pair += 2) {
                        args.pushTable();
                        args.pushInteger(flat[pair]);
                        args.setIndex(1);
                        args.pushInteger(flat[pair + 1]);
                        args.setIndex(2);
                        args.setIndex(static_cast<std::int64_t>(pair / 2) + 1);
                    }
                    return 1;
                })
        .method("block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    pushText(args, snbtOf(world->blockEntityNbt(spot.x, spot.y, spot.z)));
                    return 1;
                })
        .method("set_block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->setBlockEntityNbt(spot.x, spot.y, spot.z, bytesOf(args, spot.next)));
                    return 1;
                })
        .method("create_block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->createBlockEntity(spot.x, spot.y, spot.z,
                                                              args.isNil(spot.next) ? std::vector<std::uint8_t>{}
                                                                                    : bytesOf(args, spot.next)));
                    return 1;
                })
        .method("remove_block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->removeBlockEntity(spot.x, spot.y, spot.z));
                    return 1;
                })
        .method("remove_entity",
                [world](LuaArgs& args) {
                    args.pushBoolean(world->removeEntityByUuid(args.toText(firstArgument(args))));
                    return 1;
                })
        .method("block_entities",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> flat =
                        world->blockEntityPositions(intAt(args, base), intAt(args, base + 1));
                    args.pushTable();
                    for (std::size_t triple = 0; triple + 2 < flat.size(); triple += 3) {
                        args.pushTable();
                        for (std::size_t part = 0; part < 3; part++) {
                            args.pushInteger(flat[triple + part]);
                            args.setIndex(static_cast<std::int64_t>(part) + 1);
                        }
                        args.setIndex(static_cast<std::int64_t>(triple / 3) + 1);
                    }
                    return 1;
                })
        .method("entity_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::int32_t id = 0;
                    if (args.isTable(base)) {
                        args.pushField(base, "id");
                        id = static_cast<std::int32_t>(args.toInteger(-1));
                        args.pop(1);
                    } else {
                        id = intAt(args, base);
                    }
                    pushText(args, snbtOf(world->entityNbt(id)));
                    return 1;
                })
        .method("spawn_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::int32_t id = world->spawnEntityNbt(
                        bytesOf(args, base), args.isString(base + 1) ? args.toText(base + 1) : std::string{"plugin"});
                    const std::shared_ptr<PluginEntityRef> made = id < 0 ? nullptr : world->entityById(id);
                    if (made == nullptr) {
                        args.pushNil();
                    } else {
                        pushEntity(args, made);
                    }
                    return 1;
                })
        .method("entity",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::shared_ptr<PluginEntityRef> found = args.isString(base) && !args.isNumber(base)
                                                                       ? world->entityByUuid(args.toText(base))
                                                                       : world->entityById(intAt(args, base));
                    if (found == nullptr) {
                        args.pushNil();
                    } else {
                        pushEntity(args, found);
                    }
                    return 1;
                })
        .method("entities_in",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const std::shared_ptr<PluginEntityRef>& entity :
                         world->entitiesIn(intAt(args, base), intAt(args, base + 1))) {
                        pushEntity(args, entity);
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .method("biomes",
                [world, hooks](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::vector<std::int32_t> ids(biomeCells(*world));
                    if (!world->readBiomes(intAt(args, base), intAt(args, base + 1), ids)) {
                        args.pushNil();
                        return 1;
                    }
                    const std::vector<std::string> names =
                        hooks->biomeIds ? hooks->biomeIds() : std::vector<std::string>{};
                    args.pushTable();
                    for (std::size_t cell = 0; cell < ids.size(); cell++) {
                        const auto id = static_cast<std::size_t>(ids[cell]);
                        args.pushString(id < names.size() ? names[id] : "");
                        args.setIndex(static_cast<std::int64_t>(cell) + 1);
                    }
                    return 1;
                })
        .method("set_biomes", [world, hooks](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            if (!args.isTable(base + 2)) {
                args.raise("set_biomes(chunk_x, chunk_z, biomes) takes one biome name per cell");
            }
            std::unordered_map<std::string, std::int32_t> byName;
            const std::vector<std::string> names = hooks->biomeIds ? hooks->biomeIds() : std::vector<std::string>{};
            for (std::size_t id = 0; id < names.size(); id++) {
                byName.emplace(names[id], static_cast<std::int32_t>(id));
            }
            std::vector<std::int32_t> ids(biomeCells(*world), -1);
            for (std::size_t cell = 0; cell < ids.size(); cell++) {
                args.pushIndex(base + 2, static_cast<std::int64_t>(cell) + 1);
                if (args.isString(-1)) {
                    const auto found = byName.find(args.toText(-1));
                    if (found == byName.end()) {
                        args.raise("'" + args.toText(-1) + "' is not a biome");
                    }
                    ids[cell] = found->second;
                }
                args.pop(1);
            }
            args.pushBoolean(world->writeBiomes(intAt(args, base), intAt(args, base + 1), ids));
            return 1;
        });
}
} // namespace basalt::server::plugins
