#include "WasmHud.hpp"

#include <basalt/wire/nbt/NbtIO.hpp>

#include <charconv>

namespace basalt::server::plugins::wasm {
namespace hud {
namespace {
constexpr std::int32_t kMaxSlot = 40;

[[nodiscard]] std::shared_ptr<PluginWorldRef> worldNamed(State& state, const Value& spec) {
    std::optional<std::string> name;
    if (takeString(spec, "world", name) != abi::kOk) {
        return nullptr;
    }
    PluginWorlds& worlds = state.host->services().worlds();
    return name ? worlds.byName(*name) : worlds.defaultWorld();
}

struct Point {
    std::optional<double> x;
    std::optional<double> y;
    std::optional<double> z;

    [[nodiscard]] bool all() const { return x && y && z; }

    [[nodiscard]] bool any() const { return x || y || z; }
};

[[nodiscard]] std::int32_t takePoint(const Value& spec, Point& point) {
    std::int32_t status = takeNumber(spec, "x", point.x);
    status = status != abi::kOk ? status : takeNumber(spec, "y", point.y);
    return status != abi::kOk ? status : takeNumber(spec, "z", point.z);
}

[[nodiscard]] Value audienceValue(const std::optional<std::vector<std::string>>& audience) {
    return audience ? stringList(*audience) : Value{};
}

[[nodiscard]] std::int32_t setAudience(State& state, const Value& spec, auto& ref) {
    const Value* audience = spec.find("audience");
    if (audience == nullptr) {
        return abi::kOk;
    }
    if (audience->isNull()) {
        ref.showToAll();
        return abi::kOk;
    }
    const std::optional<std::vector<std::string>> uuids = readAudience(state, *audience);
    if (!uuids) {
        return abi::kErrInvalid;
    }
    ref.showOnlyTo(*uuids);
    return abi::kOk;
}

[[nodiscard]] std::int32_t hologramOf(State& state, std::int32_t handle, HologramEntry*& out) {
    return stateOfHud(state).holograms.find(handle, out);
}

void hologramCreate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> worldName = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> text = memory.string(c.i32(2), c.i32(3));
    if (!worldName || !text) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = state.host->services().worlds().byName(*worldName);
    std::string rendered;
    const std::int32_t rendering = renderPlain(state.plugin->text, *text, rendered);
    if (world == nullptr || rendering != abi::kOk) {
        c.ret(abi::kErrAbsent);
        return;
    }
    std::shared_ptr<PluginHologramRef> made = world->hologram(rendered, c.f64(4), c.f64(5), c.f64(6));
    HudState& hud = stateOfHud(state);
    const std::int32_t handle = made ? hud.holograms.add(HologramEntry{made, {}}) : 0;
    if (handle == 0) {
        if (made) {
            made->remove();
        }
        c.ret(abi::kErrInvalid);
        return;
    }
    HologramEntry* entry = nullptr;
    static_cast<void>(hud.holograms.find(handle, entry));
    entry->registration = state.plugin->registrations.add("hologram", [made] { made->remove(); });
    c.ret(handle);
}

void hologramSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HologramEntry* entry = nullptr;
    std::int32_t status = hologramOf(state, c.i32(0), entry);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    if (status == abi::kOk && !spec) {
        status = abi::kErrInvalid;
    }
    Point point;
    if (status == abi::kOk) {
        status = takePoint(*spec, point);
        status = status != abi::kOk || !point.any() || point.all() ? status : abi::kErrInvalid;
    }
    std::optional<std::string> text;
    if (status == abi::kOk) {
        status = takeText(state, *spec, "text", text);
        status = status == abi::kErrAbsent ? abi::kErrInvalid : status;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginHologramRef& ref = *entry->ref;
    if (text) {
        ref.setText(*text);
    }
    if (point.all() && !ref.teleport(*point.x, *point.y, *point.z)) {
        status = abi::kErrInvalid;
    }
    c.ret(status != abi::kOk ? status : setAudience(state, *spec, ref));
}

void hologramGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HologramEntry* entry = nullptr;
    const std::int32_t status = hologramOf(state, c.i32(0), entry);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const PluginHologramRef& ref = *entry->ref;
    Value::Map map;
    map.push_back({"id", Value{std::int64_t{ref.id()}}});
    map.push_back({"world", Value{ref.world()}});
    map.push_back({"x", Value{ref.x()}});
    map.push_back({"y", Value{ref.y()}});
    map.push_back({"z", Value{ref.z()}});
    map.push_back({"alive", Value{ref.alive()}});
    map.push_back({"text", Value{ref.text()}});
    map.push_back({"audience", audienceValue(ref.audience())});
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void hologramRemove(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HologramEntry* entry = nullptr;
    const std::int32_t status = hologramOf(state, c.i32(0), entry);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginHandle registration = entry->registration;
    entry->ref->remove();
    static_cast<void>(stateOfHud(state).holograms.remove(c.i32(0)));
    registration.remove();
    c.ret(abi::kOk);
}

[[nodiscard]] std::int32_t npcOf(State& state, std::int32_t handle, NpcEntry*& out) {
    return stateOfHud(state).npcs.find(handle, out);
}

[[nodiscard]] std::string attributeName(std::string_view name) {
    return name.find(':') == std::string_view::npos ? "minecraft:" + std::string{name} : std::string{name};
}

[[nodiscard]] std::int32_t applySkin(const Value& skin, PluginNpcRef& npc) {
    if (skin.asMap() == nullptr) {
        return abi::kErrInvalid;
    }
    std::optional<std::string> texture;
    std::optional<std::string> signature;
    std::optional<std::string> player;
    std::int32_t status = takeString(skin, "texture", texture);
    status = status != abi::kOk ? status : takeString(skin, "signature", signature);
    status = status != abi::kOk ? status : takeString(skin, "name", player);
    if (status != abi::kOk) {
        return status;
    }
    if (player) {
        return npc.skinFrom(*player) ? abi::kOk : abi::kErrInvalid;
    }
    if (!texture) {
        return abi::kErrInvalid;
    }
    npc.skin(*texture, signature);
    return abi::kOk;
}

[[nodiscard]] std::int32_t applyEquipment(const Value& equipment, PluginNpcRef& npc) {
    const Value::Map* slots = equipment.asMap();
    if (slots == nullptr) {
        return abi::kErrInvalid;
    }
    for (const wire::wasm::MapEntry& slotEntry : *slots) {
        std::int32_t slot = -1;
        const auto [end, error] =
            std::from_chars(slotEntry.key.data(), slotEntry.key.data() + slotEntry.key.size(), slot);
        if (error != std::errc{} || end != slotEntry.key.data() + slotEntry.key.size() || slot < 0 || slot > kMaxSlot) {
            return abi::kErrInvalid;
        }
        if (slotEntry.value.isNull()) {
            npc.setItem(slot, {});
            continue;
        }
        std::optional<std::string> item;
        std::optional<std::int64_t> count;
        std::int32_t status = takeString(slotEntry.value, "item", item);
        status = status != abi::kOk ? status : takeInt(slotEntry.value, "count", count);
        if (status != abi::kOk || !item ||
            !npc.setItemById(slot, *item, static_cast<std::int32_t>(count.value_or(1)))) {
            return abi::kErrInvalid;
        }
    }
    return abi::kOk;
}

[[nodiscard]] std::int32_t applyAttributes(const Value& attributes, PluginNpcRef& npc) {
    const Value::Map* map = attributes.asMap();
    if (map == nullptr) {
        return abi::kErrInvalid;
    }
    for (const wire::wasm::MapEntry& entry : *map) {
        const std::string name = attributeName(entry.key);
        const double* real = entry.value.asFloat();
        const std::int64_t* whole = entry.value.asInt();
        if ((real == nullptr && whole == nullptr) || !npc.attributeBase(name).has_value()) {
            return abi::kErrInvalid;
        }
        npc.setAttributeBase(name, real != nullptr ? *real : static_cast<double>(*whole));
    }
    return abi::kOk;
}

[[nodiscard]] std::int32_t applyNpcSpec(State& state, const Value& spec, PluginNpcRef& npc) {
    std::optional<std::string> name;
    std::optional<double> yaw;
    std::optional<double> pitch;
    std::optional<bool> follows;
    std::optional<double> health;
    std::optional<std::int64_t> held;
    std::optional<std::int64_t> flags;
    std::optional<std::int64_t> pose;
    Point point;
    std::int32_t status = takeText(state, spec, "name", name);
    status = status != abi::kOk ? status : takePoint(spec, point);
    status = status != abi::kOk || !point.any() || point.all() ? status : abi::kErrInvalid;
    status = status != abi::kOk ? status : takeNumber(spec, "yaw", yaw);
    status = status != abi::kOk ? status : takeNumber(spec, "pitch", pitch);
    status = status != abi::kOk ? status : takeBool(spec, "looks_at_players", follows);
    status = status != abi::kOk ? status : takeNumber(spec, "health", health);
    status = status != abi::kOk ? status : takeInt(spec, "held_slot", held);
    status = status != abi::kOk ? status : takeInt(spec, "flags", flags);
    status = status != abi::kOk ? status : takeInt(spec, "pose", pose);
    if (status != abi::kOk || (held && (*held < 0 || *held > 8))) {
        return status == abi::kErrAbsent || status == abi::kOk ? abi::kErrInvalid : status;
    }
    if (name) {
        npc.setName(*name);
    }
    if (point.all() && !npc.teleport(*point.x, *point.y, *point.z)) {
        return abi::kErrInvalid;
    }
    if (yaw || pitch) {
        npc.look(static_cast<float>(yaw.value_or(0.0)), static_cast<float>(pitch.value_or(0.0)));
    }
    if (follows) {
        npc.looksAtPlayers(*follows);
    }
    if (const Value* skin = spec.find("skin"); skin != nullptr) {
        status = applySkin(*skin, npc);
    }
    status = status != abi::kOk ? status : setAudience(state, spec, npc);
    if (status != abi::kOk) {
        return status;
    }
    if (health) {
        npc.setHealth(*health);
    }
    if (held) {
        npc.setHeld(static_cast<std::int32_t>(*held));
    }
    if (flags || pose) {
        npc.posture(static_cast<std::uint8_t>(flags.value_or(npc.flags())),
                    static_cast<std::int32_t>(pose.value_or(npc.pose())));
    }
    if (const Value* attributes = spec.find("attributes"); attributes != nullptr) {
        status = applyAttributes(*attributes, npc);
    }
    if (const Value* equipment = spec.find("equipment"); status == abi::kOk && equipment != nullptr) {
        status = applyEquipment(*equipment, npc);
    }
    return status;
}

void npcClicked(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t token,
                const std::shared_ptr<std::int32_t>& handle, const std::string& uuid) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr) {
        return;
    }
    const std::shared_ptr<PluginPlayerRef> player = host.services().players().byUuid(uuid);
    Value::Map map;
    map.push_back({"npc", Value{*handle}});
    map.push_back({"player", Value{player != nullptr ? player->name() : std::string{}}});
    map.push_back({"uuid", Value{uuid}});
    static_cast<void>(deliverValue(*state, token, Value{std::move(map)}));
}

void npcCreate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(0), c.i32(1));
    if (!spec) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::int32_t token = c.i32(2);
    std::optional<std::string> name;
    std::optional<std::string> uuid;
    Point point;
    std::int32_t status = takeText(state, *spec, "name", name);
    status = status != abi::kOk ? status : takeString(*spec, "uuid", uuid);
    status = status != abi::kOk ? status : takePoint(*spec, point);
    if (status != abi::kOk || !point.all() || !name || name->empty() || (uuid && token != 0)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<PluginWorldRef> world = worldNamed(state, *spec);
    if (world == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const std::shared_ptr<std::int32_t> handleBox = std::make_shared<std::int32_t>(0);
    std::function<void(const std::string&)> onClick;
    if (token != 0) {
        PluginHost* host = state.host;
        onClick = [host, link = state.link, token, handleBox](const std::string& who) {
            npcClicked(*host, link, token, handleBox, who);
        };
    }
    std::shared_ptr<PluginNpcRef> made = uuid ? world->fakePlayer(*uuid, *name, *point.x, *point.y, *point.z)
                                              : world->npc(*name, *point.x, *point.y, *point.z, std::move(onClick));
    HudState& hud = stateOfHud(state);
    const std::int32_t handle = made ? hud.npcs.add(NpcEntry{made, {}}) : 0;
    if (handle == 0) {
        if (made) {
            made->remove();
        }
        c.ret(abi::kErrInvalid);
        return;
    }
    *handleBox = handle;
    made->registerAttribute("minecraft:max_health", 20.0, 1.0, 1024.0);
    made->registerAttribute("minecraft:movement_speed", 0.1, 0.0, 1024.0);
    NpcEntry* entry = nullptr;
    static_cast<void>(hud.npcs.find(handle, entry));
    entry->registration = state.plugin->registrations.add("npc", [made] { made->remove(); });
    if (uuid) {
        made->place(*point.x, *point.y, *point.z, 0.0F, 0.0F, 0.0F, true);
    }
    if (const Value* skin = spec->find("skin"); skin != nullptr && applySkin(*skin, *made) != abi::kOk) {
        PluginHandle registration = entry->registration;
        static_cast<void>(hud.npcs.remove(handle));
        registration.remove();
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(handle);
}

void npcSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    NpcEntry* entry = nullptr;
    std::int32_t status = npcOf(state, c.i32(0), entry);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    if (status == abi::kOk) {
        status = spec ? applyNpcSpec(state, *spec, *entry->ref) : abi::kErrInvalid;
    }
    c.ret(status);
}

void npcGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    NpcEntry* entry = nullptr;
    const std::int32_t status = npcOf(state, c.i32(0), entry);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const PluginNpcRef& npc = *entry->ref;
    Value::Map map;
    map.push_back({"id", Value{std::int64_t{npc.id()}}});
    map.push_back({"uuid", Value{npc.uuid()}});
    map.push_back({"name", Value{npc.name()}});
    map.push_back({"world", Value{npc.world()}});
    map.push_back({"x", Value{npc.x()}});
    map.push_back({"y", Value{npc.y()}});
    map.push_back({"z", Value{npc.z()}});
    map.push_back({"alive", Value{npc.alive()}});
    map.push_back({"audience", audienceValue(npc.audience())});
    map.push_back({"health", Value{npc.health()}});
    map.push_back({"max_health", Value{npc.attributeValue("minecraft:max_health").value_or(20.0)}});
    map.push_back({"flags", Value{std::int64_t{npc.flags()}}});
    map.push_back({"pose", Value{std::int64_t{npc.pose()}}});
    map.push_back({"held_slot", Value{std::int64_t{npc.held()}}});
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void npcItem(void* user, abi::Call& c) {
    State& state = stateOf(user);
    NpcEntry* entry = nullptr;
    std::int32_t status = npcOf(state, c.i32(0), entry);
    if (status == abi::kOk && (c.i32(1) < 0 || c.i32(1) > kMaxSlot)) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::vector<std::uint8_t> bytes = entry->ref->item(c.i32(1));
    const auto parsed = wire::nbt::nbtIO::read(bytes);
    const wire::nbt::NbtCompound* stack =
        !bytes.empty() && parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
    if (stack == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    Value::Map map;
    map.push_back({"item", Value{std::string{stack->getString("id")}}});
    map.push_back({"count", Value{std::int64_t{stack->getInt("count", 1)}}});
    c.ret(putValue(c.memory(), c.i32(2), c.i32(3), Value{std::move(map)}));
}

void npcAttribute(void* user, abi::Call& c) {
    State& state = stateOf(user);
    NpcEntry* entry = nullptr;
    const std::int32_t status = npcOf(state, c.i32(0), entry);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !name) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const std::optional<double> value = entry->ref->attributeValue(attributeName(*name));
    if (!value) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(c.memory().write(c.i32(3), *value) ? abi::kOk : abi::kErrInvalid);
}

void npcAnimate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    NpcEntry* entry = nullptr;
    const std::int32_t status = npcOf(state, c.i32(0), entry);
    if (status == abi::kOk) {
        entry->ref->animate(c.i32(1));
    }
    c.ret(status);
}

void npcRemove(void* user, abi::Call& c) {
    State& state = stateOf(user);
    NpcEntry* entry = nullptr;
    const std::int32_t status = npcOf(state, c.i32(0), entry);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginHandle registration = entry->registration;
    entry->ref->remove();
    static_cast<void>(stateOfHud(state).npcs.remove(c.i32(0)));
    registration.remove();
    c.ret(abi::kOk);
}
} // namespace
} // namespace hud

void addHudActorsImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"hud_hologram_create", hud::hologramCreate, user},
                              {"hud_hologram_set", hud::hologramSet, user},
                              {"hud_hologram_get", hud::hologramGet, user},
                              {"hud_hologram_remove", hud::hologramRemove, user},
                              {"hud_npc_create", hud::npcCreate, user},
                              {"hud_npc_set", hud::npcSet, user},
                              {"hud_npc_get", hud::npcGet, user},
                              {"hud_npc_item", hud::npcItem, user},
                              {"hud_npc_attribute", hud::npcAttribute, user},
                              {"hud_npc_animate", hud::npcAnimate, user},
                              {"hud_npc_remove", hud::npcRemove, user},
                          });
}
} // namespace basalt::server::plugins::wasm
