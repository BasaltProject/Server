#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/plugins/PluginValue.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

constexpr std::int32_t kChunk = 16;
constexpr double kTicksPerSecond = 20.0;

// parity: the default `/title` fade-in, stay and fade-out, in ticks.
constexpr std::int32_t kFadeIn = 10;
constexpr std::int32_t kStay = 70;
constexpr std::int32_t kFadeOut = 20;

[[nodiscard]] std::int32_t floorDiv(double value, std::int32_t by) {
    return static_cast<std::int32_t>(std::floor(value / by));
}

[[nodiscard]] std::int32_t timing(LuaArgs& lua, std::int32_t index, std::string_view key, std::int32_t fallback) {
    if (index > lua.count() || !lua.isTable(index)) {
        return fallback;
    }
    lua.pushField(index, key);
    const std::int32_t answer = lua.isNumber(-1) ? static_cast<std::int32_t>(lua.toInteger(-1)) : fallback;
    lua.pop(1);
    return answer;
}

[[nodiscard]] std::vector<std::string> stringsOf(LuaArgs& lua, std::int32_t index) {
    std::vector<std::string> out;
    if (!lua.isTable(index)) {
        return out;
    }
    const std::int64_t length = lua.length(index);
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        out.push_back(lua.toText(-1));
        lua.pop(1);
    }
    return out;
}

[[nodiscard]] std::map<std::string, std::int32_t> levelsOf(LuaArgs& lua, std::int32_t index) {
    std::map<std::string, std::int32_t> out;
    if (!lua.isTable(index)) {
        return out;
    }
    lua.pushNil();
    while (lua.nextEntry(index)) {
        out.emplace(lua.toText(-2), static_cast<std::int32_t>(lua.toInteger(-1)));
        lua.pop(1);
    }
    return out;
}

[[nodiscard]] std::optional<std::string> storedValue(LuaArgs& args) {
    if (args.isNil(3)) {
        return std::nullopt;
    }
    std::optional<std::string> encoded = wire::plugins::encodeValue(args, 3);
    if (!encoded.has_value()) {
        args.raise("a stored value is a boolean, a number or a string");
    }
    return encoded;
}

[[nodiscard]] wire::nbt::NbtCompound componentsOf(LuaArgs& lua, std::int32_t index) {
    wire::nbt::NbtCompound out;
    if (!lua.isTable(index)) {
        return out;
    }
    lua.pushNil();
    while (lua.nextEntry(index)) {
        const std::string name = lua.toText(-2);
        const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(lua.toText(-1));
        if (!parsed.has_value()) {
            lua.raise("the component '" + name + "' is not valid NBT text");
        }
        out.set(name, *parsed);
        lua.pop(1);
    }
    return out;
}
} // namespace

std::optional<PluginPoint> pointFrom(LuaArgs& lua, std::int32_t index) {
    if (index > lua.count() || !lua.isTable(index)) {
        return std::nullopt;
    }
    lua.pushField(index, "x");
    if (!lua.isNumber(-1)) {
        lua.pop(1);
        return std::nullopt;
    }
    PluginPoint point;
    point.x = lua.toNumber(-1);
    lua.pop(1);
    lua.pushField(index, "world");
    if (lua.isString(-1)) {
        point.world = lua.toText(-1);
    }
    lua.pop(1);
    lua.pushField(index, "y");
    point.y = lua.toNumber(-1);
    lua.pop(1);
    lua.pushField(index, "z");
    point.z = lua.toNumber(-1);
    lua.pop(1);
    return point;
}

std::optional<PluginItem> itemFrom(LuaArgs& lua, std::int32_t index) {
    if (index > lua.count()) {
        return std::nullopt;
    }
    if (lua.isString(index) && !lua.isNumber(index)) {
        return PluginItem{.id = lua.toText(index)};
    }
    if (!lua.isTable(index)) {
        return std::nullopt;
    }
    lua.pushField(index, "id");
    if (!lua.isString(-1)) {
        lua.pop(1);
        lua.raise("an item needs an 'id', as in { id = \"minecraft:bread\", count = 3 }");
    }
    PluginItem item;
    item.id = lua.toText(-1);
    lua.pop(1);
    lua.pushField(index, "count");
    item.count = lua.isNumber(-1) ? static_cast<std::int32_t>(lua.toInteger(-1)) : 1;
    lua.pop(1);
    lua.pushField(index, "name");
    if (lua.isString(-1)) {
        item.name = lua.toText(-1);
    }
    lua.pop(1);
    lua.pushField(index, "damage");
    item.damage = lua.isNumber(-1) ? static_cast<std::int32_t>(lua.toInteger(-1)) : 0;
    lua.pop(1);
    lua.pushField(index, "lore");
    item.lore = stringsOf(lua, lua.top());
    lua.pop(1);
    lua.pushField(index, "enchantments");
    item.enchantments = levelsOf(lua, lua.top());
    lua.pop(1);
    lua.pushField(index, "components");
    item.components = componentsOf(lua, lua.top());
    lua.pop(1);
    return item;
}

std::shared_ptr<PluginPlayerRef> playerFrom(LuaArgs& lua, std::int32_t index, PluginPlayers& players) {
    if (index > lua.count() || !lua.isTable(index)) {
        return nullptr;
    }
    lua.pushField(index, "uuid");
    if (!lua.isString(-1)) {
        lua.pop(1);
        return nullptr;
    }
    const std::string uuid = lua.toText(-1);
    lua.pop(1);
    return players.byUuid(uuid);
}

std::string resolved(const PluginText* text, LuaArgs& lua) {
    const std::int32_t first = firstArgument(lua);
    const std::string message = lua.toText(first);
    return text == nullptr ? message : text->resolve(message, &lua, first + 1);
}

void pushPosition(LuaArgs& lua, std::string world, double x, double y, double z) {
    PluginObject object{"position"};
    object.reads("world", [world] { return PluginValue{world}; })
        .reads("x", [x] { return PluginValue{x}; })
        .reads("y", [y] { return PluginValue{y}; })
        .reads("z", [z] { return PluginValue{z}; })
        .reads("chunk_x", [x] { return PluginValue{static_cast<std::int64_t>(floorDiv(x, kChunk))}; })
        .reads("chunk_z", [z] { return PluginValue{static_cast<std::int64_t>(floorDiv(z, kChunk))}; })
        .method("offset",
                [world, x, y, z](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    pushPosition(args, world, x + args.numberOr(base, 0.0), y + args.numberOr(base + 1, 0.0),
                                 z + args.numberOr(base + 2, 0.0));
                    return 1;
                })
        .method("distance", [x, y, z](LuaArgs& args) {
            const std::optional<PluginPoint> other = pointFrom(args, firstArgument(args));
            if (!other.has_value()) {
                args.raise("distance needs another position");
            }
            const double dx = x - other->x;
            const double dy = y - other->y;
            const double dz = z - other->z;
            args.pushNumber(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
            return 1;
        });
    std::move(object).push(lua);
}

void pushItem(LuaArgs& lua, const PluginItem& item) {
    PluginObject object{"item"};
    object.reads("id", [item] { return PluginValue{item.id}; })
        .reads("count", [item] { return PluginValue{static_cast<std::int64_t>(item.count)}; })
        .reads("name", [item] { return item.name.has_value() ? PluginValue{*item.name} : PluginValue{}; })
        .reads("lore", [item] { return PluginValue{item.lore}; })
        .reads("damage", [item] { return PluginValue{static_cast<std::int64_t>(item.damage)}; })
        .pushes("enchantments",
                [item](LuaArgs& args) {
                    args.pushTable();
                    for (const auto& [id, level] : item.enchantments) {
                        args.pushInteger(level);
                        args.setField(id);
                    }
                })
        .pushes("components", [item](LuaArgs& args) {
            args.pushTable();
            for (std::size_t at = 0; at < item.components.names().size(); ++at) {
                args.pushString(wire::nbt::snbt::stringify(item.components.values()[at]));
                args.setField(item.components.names()[at]);
            }
        });
    std::move(object).push(lua);
}

void pushInventory(LuaArgs& lua, const std::shared_ptr<PluginPlayerRef>& owner) {
    PluginObject object{"inventory"};
    object.reads("size", [owner] { return PluginValue{static_cast<std::int64_t>(owner->inventory().size())}; })
        .reads("held_slot", [owner] { return PluginValue{static_cast<std::int64_t>(owner->inventory().heldSlot())}; })
        .pushes("held",
                [owner](LuaArgs& args) {
                    const std::optional<PluginItem> held =
                        owner->inventory().at(kHotbarStart + owner->inventory().heldSlot());
                    if (held.has_value()) {
                        pushItem(args, *held);
                    } else {
                        args.pushNil();
                    }
                })
        .method("get",
                [owner](LuaArgs& args) {
                    const std::optional<PluginItem> held =
                        owner->inventory().at(static_cast<std::int32_t>(args.checkInteger(firstArgument(args))));
                    if (held.has_value()) {
                        pushItem(args, *held);
                    } else {
                        args.pushNil();
                    }
                    return 1;
                })
        .method("set",
                [owner](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const auto slot = static_cast<std::int32_t>(args.checkInteger(base));
                    owner->inventory().set(slot, args.isNil(base + 1) ? std::nullopt : itemFrom(args, base + 1));
                    return 0;
                })
        .method("add",
                [owner](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::optional<PluginItem> item = itemFrom(args, base);
                    if (!item.has_value()) {
                        args.raise("inventory.add needs an item id or a table");
                    }
                    if (args.isNumber(base + 1)) {
                        item->count = static_cast<std::int32_t>(args.toInteger(base + 1));
                    }
                    args.pushInteger(owner->inventory().add(*item));
                    return 1;
                })
        .method("remove",
                [owner](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushInteger(owner->inventory().remove(args.checkString(base),
                                                               static_cast<std::int32_t>(args.integerOr(base + 1, 1))));
                    return 1;
                })
        .method("count",
                [owner](LuaArgs& args) {
                    args.pushInteger(owner->inventory().count(args.checkString(firstArgument(args))));
                    return 1;
                })
        .method("clear", [owner](LuaArgs& args) {
            static_cast<void>(args);
            owner->inventory().clear();
            return 0;
        });
    std::move(object).push(lua);
}

void pushEntity(LuaArgs& lua, std::shared_ptr<PluginEntityRef> entity) {
    PluginObject object{"entity"};
    object.reads("id", [entity] { return PluginValue{static_cast<std::int64_t>(entity->id())}; })
        .reads("type", [entity] { return PluginValue{entity->type()}; })
        .reads("world", [entity] { return PluginValue{entity->world()}; })
        .reads("x", [entity] { return PluginValue{entity->x()}; })
        .reads("y", [entity] { return PluginValue{entity->y()}; })
        .reads("z", [entity] { return PluginValue{entity->z()}; })
        .reads("health", [entity] { return PluginValue{entity->health()}; })
        .reads("alive", [entity] { return PluginValue{entity->alive()}; })
        .reads("uuid", [entity] { return PluginValue{entity->uuid()}; })
        .reads("yaw", [entity] { return PluginValue{static_cast<double>(entity->rotation()[0])}; })
        .reads("pitch", [entity] { return PluginValue{static_cast<double>(entity->rotation()[1])}; })
        .reads("on_ground", [entity] { return PluginValue{entity->onGround()}; })
        .reads("age", [entity] { return PluginValue{static_cast<std::int64_t>(entity->age())}; })
        .reads("width", [entity] { return PluginValue{entity->width()}; })
        .reads("height", [entity] { return PluginValue{entity->height()}; })
        .reads("custom_name",
               [entity] {
                   const std::optional<std::string> name = entity->customName();
                   return name.has_value() ? PluginValue{*name} : PluginValue{};
               })
        .pushes("pos",
                [entity](LuaArgs& args) { pushPosition(args, entity->world(), entity->x(), entity->y(), entity->z()); })
        .pushes("velocity",
                [entity](LuaArgs& args) {
                    const std::array<double, 3> velocity = entity->velocity();
                    args.pushTable();
                    args.pushNumber(velocity[0]);
                    args.setField("x");
                    args.pushNumber(velocity[1]);
                    args.setField("y");
                    args.pushNumber(velocity[2]);
                    args.setField("z");
                })
        .writes("health", [entity](LuaArgs& args, std::int32_t index) { entity->setHealth(args.checkNumber(index)); })
        .method("remove",
                [entity](LuaArgs& args) {
                    static_cast<void>(args);
                    entity->remove();
                    return 0;
                })
        .method("teleport", [entity](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            const std::optional<PluginPoint> point = pointFrom(args, base);
            if (point.has_value()) {
                args.pushBoolean(entity->teleport(point->x, point->y, point->z));
                return 1;
            }
            args.pushBoolean(
                entity->teleport(args.checkNumber(base), args.checkNumber(base + 1), args.checkNumber(base + 2)));
            return 1;
        });
    bindEntityOperations(object, entity);
    std::move(object).push(lua);
}

void pushPlayerData(LuaArgs& lua, PluginStorage& storage, std::string pluginId, std::string uuid) {
    PluginStorage* store = &storage;
    lua.pushTable();
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "data.index", [store, pluginId, uuid](LuaArgs& args) {
        const std::optional<std::string> stored = store->playerValue(pluginId, uuid, args.checkString(2));
        if (!stored.has_value()) {
            args.pushNil();
        } else {
            wire::plugins::pushValue(args, *stored);
        }
        return 1;
    });
    lua.setField("__index");
    wire::lua::pushHostFunction(lua, "data.newindex", [store, pluginId, uuid](LuaArgs& args) {
        const std::string key = args.checkString(2);
        store->setPlayerValue(pluginId, uuid, key, storedValue(args));
        return 0;
    });
    lua.setField("__newindex");
    lua.setMetatable(-2);
}

void pushPlayer(LuaArgs& lua, std::shared_ptr<PluginPlayerRef> player, PluginStorage* storage,
                const std::string& pluginId, const PluginText* text) {
    PluginObject object{"player"};
    object.reads("name", [player] { return PluginValue{player->name()}; })
        .reads("uuid", [player] { return PluginValue{player->uuid()}; })
        .reads("online", [player] { return PluginValue{player->online()}; })
        .reads("health", [player] { return PluginValue{player->health()}; })
        .reads("food", [player] { return PluginValue{static_cast<std::int64_t>(player->food())}; })
        .reads("gamemode", [player] { return PluginValue{player->gamemode()}; })
        .reads("level", [player] { return PluginValue{static_cast<std::int64_t>(player->level())}; })
        .reads("world", [player] { return PluginValue{player->world()}; })
        .reads("x", [player] { return PluginValue{player->x()}; })
        .reads("y", [player] { return PluginValue{player->y()}; })
        .reads("z", [player] { return PluginValue{player->z()}; })
        .reads("ping", [player] { return PluginValue{static_cast<std::int64_t>(player->ping())}; })
        .reads("sneaking", [player] { return PluginValue{player->sneaking()}; })
        .reads("held_item",
               [player] {
                   const std::optional<std::string> held = player->heldItem();
                   return held.has_value() ? PluginValue{*held} : PluginValue{};
               })
        .reads("attributes", [player] { return PluginValue{player->attributes()}; })
        .pushes("inventory", [player](LuaArgs& args) { pushInventory(args, player); })
        .pushes("pos",
                [player](LuaArgs& args) { pushPosition(args, player->world(), player->x(), player->y(), player->z()); })
        .pushes("data",
                [player, storage, pluginId](LuaArgs& args) {
                    if (storage == nullptr || pluginId.empty()) {
                        args.pushNil();
                        return;
                    }
                    pushPlayerData(args, *storage, pluginId, player->uuid());
                })
        .writes("health", [player](LuaArgs& args, std::int32_t index) { player->setHealth(args.checkNumber(index)); })
        .writes("food",
                [player](LuaArgs& args, std::int32_t index) {
                    player->setFood(static_cast<std::int32_t>(args.checkInteger(index)));
                })
        .writes("gamemode",
                [player](LuaArgs& args, std::int32_t index) { player->setGamemode(args.checkString(index)); })
        .writes("level",
                [player](LuaArgs& args, std::int32_t index) {
                    player->setLevel(static_cast<std::int32_t>(args.checkInteger(index)));
                })
        .method("send",
                [player, text](LuaArgs& args) {
                    player->send(resolved(text, args));
                    return 0;
                })
        .method("action_bar",
                [player, text](LuaArgs& args) {
                    player->actionBar(resolved(text, args));
                    return 0;
                })
        .method("title",
                [player, text](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::string subtitle;
                    if (args.isString(base + 1)) {
                        subtitle =
                            text == nullptr ? args.toText(base + 1) : text->resolve(args.toText(base + 1), nullptr, 0);
                    }
                    player->title(resolved(text, args), subtitle, timing(args, base + 2, "fade_in", kFadeIn),
                                  timing(args, base + 2, "stay", kStay), timing(args, base + 2, "fade_out", kFadeOut));
                    return 0;
                })
        .method("attribute",
                [player](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::string name = args.checkString(base);
                    if (args.isNil(base + 1)) {
                        const std::optional<double> value = player->attribute(name);
                        if (value.has_value()) {
                            args.pushNumber(*value);
                        } else {
                            args.pushNil();
                        }
                        return 1;
                    }
                    args.pushBoolean(player->setAttribute(name, args.checkNumber(base + 1)));
                    return 1;
                })
        .method("sound",
                [player](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    player->sound(args.checkString(base), static_cast<float>(args.numberOr(base + 1, 1.0)),
                                  static_cast<float>(args.numberOr(base + 2, 1.0)));
                    return 0;
                })
        .method("particle",
                [player](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::string name = args.checkString(base);
                    const std::optional<PluginPoint> point = pointFrom(args, base + 1);
                    const std::int32_t next = point.has_value() ? base + 2 : base + 1;
                    player->particle(name, point.has_value() ? point->x : player->x(),
                                     point.has_value() ? point->y : player->y(),
                                     point.has_value() ? point->z : player->z(),
                                     static_cast<std::int32_t>(args.integerOr(next, 1)), args.numberOr(next + 1, 0.0));
                    return 0;
                })
        .method("effect",
                [player](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::string name = args.checkString(base);
                    const double seconds = args.numberOr(base + 1, -1.0);
                    const std::int32_t level = static_cast<std::int32_t>(args.integerOr(base + 2, 1));
                    const std::int32_t ticks =
                        seconds < 0.0 ? -1 : static_cast<std::int32_t>(seconds * kTicksPerSecond);
                    args.pushBoolean(player->effect(name, ticks, level > 0 ? level - 1 : 0));
                    return 1;
                })
        .method("kick",
                [player, text](LuaArgs& args) {
                    player->kick(resolved(text, args));
                    return 0;
                })
        .method("may",
                [player](LuaArgs& args) {
                    args.pushBoolean(player->may(args.checkString(firstArgument(args))));
                    return 1;
                })
        .method("grant",
                [player, pluginId](LuaArgs& args) {
                    player->setPermission(pluginId, args.checkString(firstArgument(args)), true);
                    return 0;
                })
        .method("deny",
                [player, pluginId](LuaArgs& args) {
                    player->setPermission(pluginId, args.checkString(firstArgument(args)), false);
                    return 0;
                })
        .method("revoke",
                [player, pluginId](LuaArgs& args) {
                    player->setPermission(pluginId, args.checkString(firstArgument(args)), std::nullopt);
                    return 0;
                })
        .method("give",
                [player](LuaArgs& args) {
                    const std::int32_t first = firstArgument(args);
                    args.pushBoolean(
                        player->give(args.checkString(first), static_cast<std::int32_t>(args.integerOr(first + 1, 1))));
                    return 1;
                })
        .method("teleport", [player](LuaArgs& args) {
            const std::int32_t first = firstArgument(args);
            const std::optional<PluginPoint> point = pointFrom(args, first);
            if (point.has_value()) {
                args.pushBoolean(
                    player->teleport(point->world.value_or(player->world()), point->x, point->y, point->z));
                return 1;
            }
            const bool named = args.isString(first) && !args.isNumber(first);
            const std::string world = named ? args.toText(first) : player->world();
            const std::int32_t base = named ? first + 1 : first;
            args.pushBoolean(player->teleport(world, args.checkNumber(base), args.checkNumber(base + 1),
                                              args.checkNumber(base + 2)));
            return 1;
        });
    bindPlayerOperations(object, player);
    std::move(object).push(lua);
}
} // namespace basalt::server::plugins
