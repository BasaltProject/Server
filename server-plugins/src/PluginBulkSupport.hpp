#pragma once

#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::server::plugins::bulk {
using wire::lua::LuaArgs;

constexpr std::size_t kSectionBlocks = 4096;

[[nodiscard]] inline std::int32_t intAt(LuaArgs& lua, std::int32_t index) {
    return static_cast<std::int32_t>(lua.checkInteger(index));
}

[[nodiscard]] inline std::vector<std::int32_t> statesFrom(LuaArgs& lua, std::int32_t index, std::size_t expected) {
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

inline void pushInts(LuaArgs& lua, std::span<const std::int32_t> values) {
    lua.pushTable();
    for (std::size_t position = 0; position < values.size(); position++) {
        lua.pushInteger(values[position]);
        lua.setIndex(static_cast<std::int64_t>(position) + 1);
    }
}

/// A list of `{ chunk_x, chunk_z }` pairs, flattened to x, z, x, z...
[[nodiscard]] inline std::vector<std::int32_t> chunkPairs(LuaArgs& lua, std::int32_t index) {
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

[[nodiscard]] inline bool flag(LuaArgs& lua, std::int32_t options, std::string_view key, bool fallback) {
    if (!lua.isTable(options)) {
        return fallback;
    }
    lua.pushField(options, key);
    const bool value = lua.isNil(-1) ? fallback : lua.toBoolean(-1);
    lua.pop(1);
    return value;
}

[[nodiscard]] inline std::size_t biomeCells(const PluginWorldRef& world) {
    return static_cast<std::size_t>(std::max(world.height(), 0) / 4) * 16;
}

struct Sections {
    std::vector<std::int32_t> ys;
    std::vector<std::int32_t> states;
};

/// A table of section_y to a list of 4096 state ids, taken in ascending section order.
[[nodiscard]] inline Sections sectionsFrom(LuaArgs& lua, std::int32_t index) {
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

inline void pushPrevious(LuaArgs& lua, std::span<const std::int32_t> ys, std::span<const std::int32_t> previous) {
    lua.pushTable();
    for (std::size_t section = 0; section < ys.size(); section++) {
        pushInts(lua, previous.subspan(section * kSectionBlocks, kSectionBlocks));
        lua.setIndex(ys[section]);
    }
}

[[nodiscard]] inline std::optional<std::string> snbtOf(const std::optional<std::vector<std::uint8_t>>& bytes) {
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    if (!read.has_value()) {
        return std::nullopt;
    }
    return wire::nbt::snbt::stringify(read->tag);
}

[[nodiscard]] inline std::vector<std::uint8_t> bytesOf(LuaArgs& lua, std::int32_t index) {
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(lua.checkString(index));
    if (!parsed.has_value()) {
        lua.raise("that is not valid NBT text");
    }
    return wire::nbt::nbtIO::write(*parsed);
}

inline void pushText(LuaArgs& lua, const std::optional<std::string>& text) {
    if (text.has_value()) {
        lua.pushString(*text);
    } else {
        lua.pushNil();
    }
}

[[nodiscard]] inline std::int32_t entityIdAt(LuaArgs& lua, std::int32_t index) {
    if (!lua.isTable(index)) {
        return static_cast<std::int32_t>(lua.toInteger(index));
    }
    lua.pushField(index, "id");
    const auto id = static_cast<std::int32_t>(lua.toInteger(-1));
    lua.pop(1);
    return id;
}

struct Spot {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t next = 0;
};

[[nodiscard]] inline Spot spotAt(LuaArgs& lua, std::int32_t base) {
    const std::optional<PluginPoint> point = pointFrom(lua, base);
    if (point.has_value()) {
        return {static_cast<std::int32_t>(std::floor(point->x)), static_cast<std::int32_t>(std::floor(point->y)),
                static_cast<std::int32_t>(std::floor(point->z)), base + 1};
    }
    return {intAt(lua, base), intAt(lua, base + 1), intAt(lua, base + 2), base + 3};
}

/// A list of { x, y, z } triples, flattened to x, y, z, x, y, z...
[[nodiscard]] inline std::vector<std::int32_t> blockTriples(LuaArgs& lua, std::int32_t index) {
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
[[nodiscard]] inline std::vector<std::int32_t> stateList(LuaArgs& lua, std::int32_t index, std::size_t count) {
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
[[nodiscard]] inline std::int32_t faceFrom(LuaArgs& lua, std::int32_t index) {
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
[[nodiscard]] inline std::int32_t layerFrom(LuaArgs& lua, std::int32_t index) {
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
[[nodiscard]] inline std::uint32_t effectsFrom(LuaArgs& lua, std::int32_t index) {
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

[[nodiscard]] inline std::string_view statusName(std::int32_t status) {
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
} // namespace basalt::server::plugins::bulk

namespace basalt::server::plugins {
void addBulkBlockEntityMethods(PluginObject& object, const std::shared_ptr<PluginWorldRef>& world,
                               const std::shared_ptr<WorldHooks>& hooks);
}
