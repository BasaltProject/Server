#include "WasmEntities.hpp"

#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <cstring>

namespace basalt::server::plugins::wasm {
namespace {
using entities::Target;
using entities::withNamespaceId;
using wire::wasm::Value;

[[nodiscard]] bool looksLikeUuid(std::string_view text) {
    if (text.size() != 36) {
        return false;
    }
    for (std::size_t at = 0; at < text.size(); ++at) {
        const bool dash = at == 8 || at == 13 || at == 18 || at == 23;
        if (dash ? text[at] != '-' : std::isxdigit(static_cast<unsigned char>(text[at])) == 0) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<double> numberOf(const Value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* whole = value->asInt()) {
        return static_cast<double>(*whole);
    }
    if (const double* real = value->asFloat()) {
        return *real;
    }
    return std::nullopt;
}

/// The options map of an import; an empty buffer is an empty map and anything but a map is INVALID.
[[nodiscard]] std::expected<Value, std::int32_t> readMap(const abi::GuestMemory& memory, std::int32_t ptr,
                                                         std::int32_t len) {
    if (len == 0) {
        return Value{Value::Map{}};
    }
    std::expected<Value, std::int32_t> value = readValue(memory, ptr, len);
    if (value && value->asMap() == nullptr) {
        return std::unexpected(abi::kErrInvalid);
    }
    return value;
}

void entityOfPlayer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : entities::table(state).handleFor(player));
}

void entityPlayer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::shared_ptr<PluginPlayerRef> player = entities::playerOf(state, *target);
    c.ret(player == nullptr ? abi::kErrAbsent : state.playerHandle(player));
}

void entityRelease(void* user, abi::Call& c) {
    c.ret(entities::table(stateOf(user)).release(c.i32(0)) ? abi::kOk : abi::kErrStale);
}

void entityAlive(void* user, abi::Call& c) {
    Target* found = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), found);
    c.ret(status == abi::kOk ? 1 : status == abi::kErrStale ? 0 : status);
}

void entityId(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    c.ret(status != abi::kOk ? status : target->id());
}

void entityById(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!name) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = entities::worldNamed(state, *name);
    const std::shared_ptr<PluginEntityRef> found = world == nullptr ? nullptr : world->entityById(c.i32(2));
    c.ret(found == nullptr || !found->alive() ? abi::kErrAbsent : entities::table(state).handleFor(found));
}

void entityByUuid(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> uuid = c.memory().string(c.i32(0), c.i32(1));
    if (!uuid || !looksLikeUuid(*uuid)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    for (const std::shared_ptr<PluginWorldRef>& world : state.host->services().worlds().all()) {
        if (const std::shared_ptr<PluginEntityRef> found = world->entityByUuid(*uuid);
            found != nullptr && found->alive()) {
            c.ret(entities::table(state).handleFor(found));
            return;
        }
    }
    const std::shared_ptr<PluginPlayerRef> player = state.host->services().players().byUuid(*uuid);
    c.ret(player == nullptr || !player->online() ? abi::kErrAbsent : entities::table(state).handleFor(player));
}

struct Query {
    std::vector<std::string> types;
    std::optional<std::array<double, 4>> sphere;
    std::optional<std::array<double, 6>> box;
    std::optional<std::array<std::int32_t, 2>> chunk;
    std::size_t limit = SIZE_MAX;
};

[[nodiscard]] std::expected<Query, std::int32_t> parseQuery(const Value& map) {
    Query query;
    if (const Value* type = map.find("type"); type != nullptr) {
        if (const std::string* one = type->asString()) {
            query.types.push_back(withNamespaceId(*one));
        } else if (const Value::List* list = type->asList()) {
            for (const Value& item : *list) {
                const std::string* text = item.asString();
                if (text == nullptr) {
                    return std::unexpected(abi::kErrInvalid);
                }
                query.types.push_back(withNamespaceId(*text));
            }
        } else {
            return std::unexpected(abi::kErrInvalid);
        }
    }
    const auto all = [&map](auto... keys) {
        return std::array<std::optional<double>, sizeof...(keys)>{numberOf(map.find(keys))...};
    };
    const auto complete = [](const auto& values) {
        return std::ranges::all_of(values, [](const std::optional<double>& one) { return one.has_value(); });
    };
    const auto sphere = all("x", "y", "z", "radius");
    if (complete(sphere)) {
        query.sphere = std::array<double, 4>{*sphere[0], *sphere[1], *sphere[2], *sphere[3]};
    }
    const auto box = all("min_x", "min_y", "min_z", "max_x", "max_y", "max_z");
    if (complete(box)) {
        query.box = std::array<double, 6>{*box[0], *box[1], *box[2], *box[3], *box[4], *box[5]};
    }
    const auto chunk = all("chunk_x", "chunk_z");
    if (complete(chunk)) {
        query.chunk =
            std::array<std::int32_t, 2>{static_cast<std::int32_t>(*chunk[0]), static_cast<std::int32_t>(*chunk[1])};
    }
    if (const std::optional<double> limit = numberOf(map.find("limit")); limit.has_value()) {
        if (*limit < 0) {
            return std::unexpected(abi::kErrInvalid);
        }
        query.limit = static_cast<std::size_t>(*limit);
    }
    return query;
}

[[nodiscard]] bool matches(const Query& query, const PluginEntityRef& entity) {
    if (!entity.alive()) {
        return false;
    }
    if (!query.types.empty() && std::ranges::find(query.types, entity.type()) == query.types.end()) {
        return false;
    }
    if (query.sphere.has_value()) {
        const double dx = entity.x() - (*query.sphere)[0];
        const double dy = entity.y() - (*query.sphere)[1];
        const double dz = entity.z() - (*query.sphere)[2];
        if (dx * dx + dy * dy + dz * dz > (*query.sphere)[3] * (*query.sphere)[3]) {
            return false;
        }
    }
    if (query.box.has_value()) {
        const std::array<double, 6>& box = *query.box;
        if (entity.x() < box[0] || entity.y() < box[1] || entity.z() < box[2] || entity.x() > box[3] ||
            entity.y() > box[4] || entity.z() > box[5]) {
            return false;
        }
    }
    return true;
}

void entityQuery(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> map = readMap(memory, c.i32(2), c.i32(3));
    if (!name || !map || c.i32(5) < 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::expected<Query, std::int32_t> query = parseQuery(*map);
    if (!query) {
        c.ret(query.error());
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = entities::worldNamed(state, *name);
    if (world == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const std::vector<std::shared_ptr<PluginEntityRef>> candidates =
        query->chunk.has_value() ? world->entitiesIn((*query->chunk)[0], (*query->chunk)[1]) : world->entities();
    std::vector<std::int32_t> handles;
    for (const std::shared_ptr<PluginEntityRef>& entity : candidates) {
        if (handles.size() < query->limit && matches(*query, *entity)) {
            if (const std::int32_t handle = entities::table(state).handleFor(entity); handle > 0) {
                handles.push_back(handle);
            }
        }
    }
    const auto count = static_cast<std::int32_t>(handles.size());
    if (count > c.i32(5)) {
        c.ret(count);
        return;
    }
    const auto view = memory.range(static_cast<std::uint32_t>(c.i32(4)), static_cast<std::uint32_t>(count) * 4U);
    if (!view) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (count > 0) {
        std::memcpy(view->data(), handles.data(), view->size());
    }
    c.ret(count);
}

void entityTypes(void* user, abi::Call& c) {
    Value::List ids;
    for (const std::string& id : stateOf(user).host->services().worlds().entityTypeIds()) {
        ids.emplace_back(id);
    }
    c.ret(putValue(c.memory(), c.i32(0), c.i32(1), Value{std::move(ids)}));
}

void entityTypeAttributes(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> type = c.memory().string(c.i32(0), c.i32(1));
    if (!type || type->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const PluginWorlds& worlds = state.host->services().worlds();
    const std::vector<std::string> known = worlds.entityTypeIds();
    if (!known.empty() && std::ranges::find(known, withNamespaceId(*type)) == known.end()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    Value::Map attributes;
    for (const std::string& entry : worlds.entityDefaultAttributes(*type)) {
        if (const std::size_t split = entry.rfind('='); split != std::string::npos) {
            attributes.push_back({entry.substr(0, split), Value{std::strtod(entry.c_str() + split + 1, nullptr)}});
        }
    }
    c.ret(putValue(c.memory(), c.i32(2), c.i32(3), Value{std::move(attributes)}));
}

void entitySpawn(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> worldName = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> type = memory.string(c.i32(2), c.i32(3));
    const std::expected<Value, std::int32_t> options = readMap(memory, c.i32(7), c.i32(8));
    if (!worldName || !type || type->empty() || !options) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = entities::worldNamed(state, *worldName);
    const std::vector<std::string> known = state.host->services().worlds().entityTypeIds();
    if (world == nullptr || (!known.empty() && std::ranges::find(known, withNamespaceId(*type)) == known.end())) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::string* reason = options->find("reason") != nullptr ? options->find("reason")->asString() : nullptr;
    const std::shared_ptr<PluginEntityRef> made = world->spawnAs(
        *type, c.f64(4), c.f64(5), c.f64(6), static_cast<float>(numberOf(options->find("yaw")).value_or(0.0)),
        reason != nullptr && !reason->empty() ? std::string_view{*reason} : std::string_view{"plugin"});
    c.ret(made == nullptr ? abi::kErrAbsent : entities::table(state).handleFor(made));
}

void entitySpawnNbt(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> worldName = memory.string(c.i32(0), c.i32(1));
    const auto bytes =
        memory.range(static_cast<std::uint32_t>(c.i32(2)), static_cast<std::uint32_t>(std::max(0, c.i32(3))));
    const std::optional<std::string_view> cause = memory.string(c.i32(4), c.i32(5));
    const std::shared_ptr<PluginWorldRef> world = worldName ? entities::worldNamed(state, *worldName) : nullptr;
    if (!worldName || !bytes || !cause || c.i32(3) <= 0 || world == nullptr ||
        !wire::nbt::nbtIO::read(std::span<const std::uint8_t>{bytes->data(), bytes->size()}).has_value()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::int32_t id = world->spawnEntityNbt({bytes->data(), bytes->size()}, cause->empty() ? "plugin" : *cause);
    const std::shared_ptr<PluginEntityRef> made = id < 0 ? nullptr : world->entityById(id);
    c.ret(made == nullptr ? abi::kErrAbsent : entities::table(state).handleFor(made));
}

void entityDropItem(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> worldName = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> item = readValue(memory, c.i32(2), c.i32(3));
    const std::shared_ptr<PluginWorldRef> world = worldName ? entities::worldNamed(state, *worldName) : nullptr;
    if (!worldName || !item || item->asMap() == nullptr || world == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::vector<std::uint8_t> stack;
    if (const Value* saved = item->find("nbt"); saved != nullptr && saved->asBytes() != nullptr) {
        stack = saved->asBytes()->data;
    } else {
        const Value* id = item->find("id");
        const std::optional<double> count = numberOf(item->find("count"));
        if (id == nullptr || id->asString() == nullptr || id->asString()->empty() ||
            (count.has_value() && *count < 1)) {
            c.ret(abi::kErrInvalid);
            return;
        }
        wire::nbt::NbtCompound compound;
        compound.putString("id", withNamespaceId(*id->asString()));
        compound.putInt("count", static_cast<std::int32_t>(count.value_or(1.0)));
        stack = wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(compound)));
    }
    const std::int32_t id = world->dropItem(stack, c.f64(4), c.f64(5), c.f64(6), c.f64(7), c.f64(8), c.f64(9));
    const std::shared_ptr<PluginEntityRef> made = id < 0 ? nullptr : world->entityById(id);
    c.ret(made == nullptr ? abi::kErrAbsent : entities::table(state).handleFor(made));
}

void entityRemove(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    if (target->isPlayer()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    target->entity->remove();
    c.ret(abi::kOk);
}

void entityRemoveUuid(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> worldName = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> uuid = memory.string(c.i32(2), c.i32(3));
    const std::shared_ptr<PluginWorldRef> world = worldName ? entities::worldNamed(state, *worldName) : nullptr;
    if (!uuid || !looksLikeUuid(*uuid) || world == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(world->removeEntityByUuid(*uuid) ? abi::kOk : abi::kErrAbsent);
}

void entityPosition(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    c.ret(c.memory().write(c.i32(1), target->position()) ? abi::kOk : abi::kErrInvalid);
}

void entityTeleport(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> options = readMap(c.memory(), c.i32(4), c.i32(5));
    Target* target = nullptr;
    std::int32_t status = entities::resolve(state, c.i32(0), target);
    if (status == abi::kOk && !options) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const Value* worldOption = options->find("world");
    const std::string* worldName = worldOption != nullptr ? worldOption->asString() : nullptr;
    if (worldName != nullptr && entities::worldNamed(state, *worldName) == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const bool otherWorld = worldName != nullptr && *worldName != target->world();
    if (otherWorld && target->player == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::array<float, 2> facing = target->entity != nullptr
                                            ? target->entity->rotation()
                                            : std::array<float, 2>{target->player->yaw(), target->player->pitch()};
    const auto yaw = static_cast<float>(numberOf(options->find("yaw")).value_or(facing[0]));
    const auto pitch = static_cast<float>(numberOf(options->find("pitch")).value_or(facing[1]));
    bool moved = false;
    if (target->player != nullptr) {
        moved = target->player->teleport(worldName != nullptr ? *worldName : target->player->world(), c.f64(1),
                                         c.f64(2), c.f64(3), yaw, pitch);
    } else {
        moved = target->entity->teleport(c.f64(1), c.f64(2), c.f64(3));
        if (moved && (options->find("yaw") != nullptr || options->find("pitch") != nullptr)) {
            static_cast<void>(target->entity->setRotation(yaw, pitch));
        }
    }
    c.ret(moved ? abi::kOk : abi::kErrDenied);
}

void entityVelocity(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk || target->isPlayer()) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(c.memory().write(c.i32(1), target->entity->velocity()) ? abi::kOk : abi::kErrInvalid);
}

void entitySetVelocity(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk || target->entity == nullptr) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(target->entity->setVelocity(c.f64(1), c.f64(2), c.f64(3)) ? abi::kOk : abi::kErrInvalid);
}

void entityRotation(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::array<float, 2> facing = target->entity != nullptr
                                            ? target->entity->rotation()
                                            : std::array<float, 2>{target->player->yaw(), target->player->pitch()};
    c.ret(c.memory().write(c.i32(1),
                           std::array<double, 2>{static_cast<double>(facing[0]), static_cast<double>(facing[1])})
              ? abi::kOk
              : abi::kErrInvalid);
}

void entitySetRotation(void* user, abi::Call& c) {
    Target* target = nullptr;
    const std::int32_t status = entities::resolve(stateOf(user), c.i32(0), target);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const auto yaw = static_cast<float>(c.f64(1));
    const auto pitch = static_cast<float>(c.f64(2));
    const bool turned = target->entity != nullptr
                            ? target->entity->setRotation(yaw, pitch)
                            : target->player->teleport(target->player->world(), target->player->x(),
                                                       target->player->y(), target->player->z(), yaw, pitch);
    c.ret(turned ? abi::kOk : abi::kErrInvalid);
}
} // namespace

void addEntitiesImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"entity_of_player", entityOfPlayer, user},
                              {"entity_player", entityPlayer, user},
                              {"entity_release", entityRelease, user},
                              {"entity_alive", entityAlive, user},
                              {"entity_id", entityId, user},
                              {"entity_by_id", entityById, user},
                              {"entity_by_uuid", entityByUuid, user},
                              {"entity_query", entityQuery, user},
                              {"entity_types", entityTypes, user},
                              {"entity_type_attributes", entityTypeAttributes, user},
                              {"entity_spawn", entitySpawn, user},
                              {"entity_spawn_nbt", entitySpawnNbt, user},
                              {"entity_drop_item", entityDropItem, user},
                              {"entity_remove", entityRemove, user},
                              {"entity_remove_uuid", entityRemoveUuid, user},
                              {"entity_position", entityPosition, user},
                              {"entity_teleport", entityTeleport, user},
                              {"entity_velocity", entityVelocity, user},
                              {"entity_set_velocity", entitySetVelocity, user},
                              {"entity_rotation", entityRotation, user},
                              {"entity_set_rotation", entitySetRotation, user},
                          });
}
} // namespace basalt::server::plugins::wasm
