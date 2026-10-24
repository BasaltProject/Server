#pragma once

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/FishingHooks.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/MobStateOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::runtime::plugins::entityOps::internal {
using entity::ServerEntity;
using server::plugins::PluginEntityValue;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using Value = PluginEntityValue;
using Arguments = std::vector<Value>;

constexpr float kDefaultDropChance = 0.085F;

[[nodiscard]] inline bool asBool(const Value& value, bool fallback = false) {
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole != 0;
    }
    return fallback;
}

[[nodiscard]] inline std::int64_t asInt(const Value& value, std::int64_t fallback = 0) {
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole;
    }
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return static_cast<std::int64_t>(*real);
    }
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag ? 1 : 0;
    }
    return fallback;
}

[[nodiscard]] inline double asReal(const Value& value, double fallback = 0.0) {
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return *real;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    return fallback;
}

[[nodiscard]] inline std::optional<std::string> asText(const Value& value) {
    const auto* text = std::get_if<std::string>(&value);
    return text == nullptr ? std::nullopt : std::optional{*text};
}

[[nodiscard]] inline const Value& at(const Arguments& arguments, std::size_t index) {
    static const Value kNone;
    return index < arguments.size() ? arguments[index] : kNone;
}

[[nodiscard]] inline Identifier idOf(std::string_view text) {
    const std::string full =
        text.find(':') == std::string_view::npos ? "minecraft:" + std::string{text} : std::string{text};
    const auto parsed = Identifier::parse(full);
    return parsed.has_value() ? *parsed : Identifier::vanilla("generic");
}

[[nodiscard]] inline std::string attributeKey(std::string_view text) {
    return text.find(':') == std::string_view::npos ? "minecraft:" + std::string{text} : std::string{text};
}

inline NbtCompound& source(ServerEntity& entity) {
    if (!entity.sourceNbt.has_value()) {
        entity.sourceNbt.emplace();
    }
    return *entity.sourceNbt;
}

struct Accessor {
    std::function<Value(const ServerEntity&)> read;
    std::function<bool(ServerEntity&, const Value&)> write;
};

[[nodiscard]] inline Value idOrNone(const std::optional<std::int32_t>& id) {
    return static_cast<std::int64_t>(id.value_or(-1));
}

using Accessors = std::map<std::string_view, Accessor, std::less<>>;

const Accessors& accessors();
} // namespace basalt::server::runtime::plugins::entityOps::internal
