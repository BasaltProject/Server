#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::plugins {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using server::plugins::PluginInventory;
using server::plugins::PluginItem;
using server::plugins::PluginPlayerRef;
using wire::common::Identifier;

constexpr std::int32_t kMaxFood = 20;
constexpr std::string_view kMaxHealthId = "minecraft:max_health";

/// The name a plugin writes, as the enum. Not the API's to expose.
[[nodiscard]] std::optional<play::Gamemode> gamemodeOf(std::string_view name) {
    if (name == "survival") {
        return play::Gamemode::Survival;
    }
    if (name == "creative") {
        return play::Gamemode::Creative;
    }
    if (name == "adventure") {
        return play::Gamemode::Adventure;
    }
    if (name == "spectator") {
        return play::Gamemode::Spectator;
    }
    return std::nullopt;
}

[[nodiscard]] std::string_view gamemodeName(play::Gamemode mode) {
    switch (mode) {
    case play::Gamemode::Survival:
        return "survival";
    case play::Gamemode::Creative:
        return "creative";
    case play::Gamemode::Adventure:
        return "adventure";
    case play::Gamemode::Spectator:
        return "spectator";
    }
    return "survival";
}

/// `max_health` and `minecraft:max_health` are both written by plugins.
[[nodiscard]] std::string qualified(std::string_view name) {
    return name.find(':') != std::string_view::npos ? std::string{name} : "minecraft:" + std::string{name};
}

[[nodiscard]] PluginItem itemOf(const storage::StoredItemStack& stack) {
    PluginItem item{.id = stack.id, .count = stack.count};
    if (stack.components.contains(item::itemComponents::kCustomName)) {
        item.name = std::string{stack.components.getString(item::itemComponents::kCustomName)};
    }
    item.damage = stack.components.getInt(item::itemComponents::kDamage);
    for (const auto& [id, level] : player::enchantmentLevels(stack)) {
        item.enchantments.emplace(id.toString(), level);
    }
    return item;
}

[[nodiscard]] storage::StoredItemStack storedOf(const PluginItem& item) {
    wire::nbt::NbtCompound components;
    if (item.name.has_value()) {
        components.set(item::itemComponents::kCustomName, wire::nbt::NbtTag::ofString(*item.name));
    }
    if (item.damage > 0) {
        components.set(item::itemComponents::kDamage, wire::nbt::NbtTag::ofInt(item.damage));
    }
    if (!item.enchantments.empty()) {
        wire::nbt::NbtCompound levels;
        for (const auto& [id, level] : item.enchantments) {
            levels.set(id, wire::nbt::NbtTag::ofInt(level));
        }
        components.set(item::itemComponents::kEnchantments, wire::nbt::NbtTag::ofCompound(levels));
    }
    return storage::StoredItemStack{
        .id = item.id, .count = std::max(item.count, 1), .components = std::move(components)};
}

/// A player's slots, through the same inventory the client is looking at.
///
/// Every write re-sends the window, because the client keeps its own copy and a slot changed behind
/// its back is a ghost item until the next click. Offline is not an error here: the handle answers
/// an empty inventory and drops the writes, the same way `health` reads 0 rather than throwing.
class SessionInventoryRef final : public PluginInventory {
public:
    explicit SessionInventoryRef(std::function<player::PlayerSession*()> session) : session_(std::move(session)) {}

    [[nodiscard]] std::int32_t size() const override { return storage::StoredInventory::kPlayerSlots; }

    [[nodiscard]] std::int32_t heldSlot() const override {
        const player::PlayerSession* player = session_();
        return player == nullptr ? 0 : player->inventory.heldSlot();
    }

    [[nodiscard]] std::optional<PluginItem> at(std::int32_t slot) const override {
        const player::PlayerSession* player = session_();
        if (player == nullptr) {
            return std::nullopt;
        }
        const std::optional<storage::StoredItemStack>& stack = player->inventory.at(slot);
        return stack.has_value() ? std::optional{itemOf(*stack)} : std::nullopt;
    }

    void set(std::int32_t slot, const std::optional<PluginItem>& item) override {
        player::PlayerSession* player = session_();
        if (player == nullptr) {
            return;
        }
        player->inventory.set(slot, item.has_value() ? std::optional{storedOf(*item)} : std::nullopt);
        if (player->connection().sendInventoryAll) {
            player->connection().sendInventoryAll();
        }
    }

    std::int32_t add(const PluginItem& item) override {
        player::PlayerSession* player = session_();
        if (player == nullptr) {
            return item.count;
        }
        const std::int32_t left = player->inventory.add(storedOf(item));
        if (player->connection().sendInventoryAll) {
            player->connection().sendInventoryAll();
        }
        return left;
    }

    std::int32_t remove(std::string_view id, std::int32_t count) override {
        player::PlayerSession* player = session_();
        if (player == nullptr) {
            return 0;
        }
        const std::int32_t taken = player->inventory.take(id, count);
        if (taken > 0 && player->connection().sendInventoryAll) {
            player->connection().sendInventoryAll();
        }
        return taken;
    }

    [[nodiscard]] std::int32_t count(std::string_view id) const override {
        const player::PlayerSession* player = session_();
        return player == nullptr ? 0 : player->inventory.countOf(id);
    }

    void clear() override {
        player::PlayerSession* player = session_();
        if (player == nullptr) {
            return;
        }
        player->inventory.clear();
        if (player->connection().sendInventoryAll) {
            player->connection().sendInventoryAll();
        }
    }

private:
    std::function<player::PlayerSession*()> session_;
};

class SessionPlayerRef final : public PluginPlayerRef {
public:
    SessionPlayerRef(player::PlayerRegistry& registry, std::string uuid, std::string fallbackName, ModelHooks hooks)
        : registry_(&registry), uuid_(std::move(uuid)), fallbackName_(std::move(fallbackName)),
          hooks_(std::move(hooks)), inventory_([this] { return session(); }) {}

    [[nodiscard]] std::string name() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? fallbackName_ : player->name();
    }

    [[nodiscard]] std::string uuid() const override { return uuid_; }

    [[nodiscard]] bool online() const override { return session() != nullptr; }

    [[nodiscard]] double health() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : static_cast<double>(player->health());
    }

    void setHealth(double value) override {
        player::PlayerSession* player = session();
        if (player == nullptr) {
            return;
        }
        player->setHealth(std::clamp(static_cast<float>(value), 0.0F, player->maxHealth()));
        if (player->connection().syncHealth) {
            player->connection().syncHealth();
        }
    }

    [[nodiscard]] std::int32_t food() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0 : player->food.foodLevel();
    }

    void setFood(std::int32_t value) override {
        player::PlayerSession* player = session();
        if (player == nullptr) {
            return;
        }
        player->food.set(std::clamp(value, 0, kMaxFood), player->food.saturationLevel());
        if (player->connection().syncHealth) {
            player->connection().syncHealth();
        }
    }

    [[nodiscard]] std::string gamemode() const override {
        const player::PlayerSession* player = session();
        return std::string{player == nullptr ? "survival" : gamemodeName(player->gamemode)};
    }

    void setGamemode(std::string_view value) override {
        player::PlayerSession* player = session();
        const std::optional<play::Gamemode> mode = gamemodeOf(value);
        if (player == nullptr || !mode.has_value() || !player->connection().setGamemode) {
            return;
        }
        player->connection().setGamemode(*mode);
    }

    [[nodiscard]] std::int32_t level() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0 : player->experience.level();
    }

    void setLevel(std::int32_t value) override {
        player::PlayerSession* player = session();
        if (player == nullptr) {
            return;
        }
        player->experience.setLevels(value);
        if (player->connection().syncExperience) {
            player->connection().syncExperience();
        }
    }

    [[nodiscard]] std::string world() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? "" : player->worldName;
    }

    [[nodiscard]] double x() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : player->x;
    }

    [[nodiscard]] double y() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : player->y;
    }

    [[nodiscard]] double z() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : player->z;
    }

    /// No latency is measured yet, so this answers zero rather than a number that means nothing.
    [[nodiscard]] std::int32_t ping() const override { return 0; }

    [[nodiscard]] bool sneaking() const override {
        const player::PlayerSession* player = session();
        return player != nullptr && player->sneaking;
    }

    void send(std::string_view message) override {
        player::PlayerSession* player = session();
        if (player != nullptr && player->connection().sendSystemMessage) {
            player->connection().sendSystemMessage(wire::text::miniMessage::parse(std::string{message}));
        }
    }

    void actionBar(std::string_view message) override {
        player::PlayerSession* player = session();
        if (player != nullptr && player->connection().sendActionBar) {
            player->connection().sendActionBar(wire::text::miniMessage::parse(std::string{message}));
        }
    }

    bool teleport(std::string_view world, double x, double y, double z) override {
        player::PlayerSession* player = session();
        if (player == nullptr) {
            return false;
        }
        // Through the same two paths a command uses, so a world change is a world change and a move
        // within one is a move.
        if (world != player->worldName) {
            world::LoadedWorld* target = hooks_.worldFor ? hooks_.worldFor(std::string{world}) : nullptr;
            if (target == nullptr || !player->connection().switchWorld) {
                return false;
            }
            return player->connection().switchWorld(*target, x, y, z);
        }
        if (!player->connection().teleportTo) {
            return false;
        }
        player->connection().teleportTo(x, y, z, player->yaw, player->pitch);
        return true;
    }

    void kick(std::string_view reason) override {
        player::PlayerSession* player = session();
        if (player != nullptr && player->connection().disconnect) {
            player->connection().disconnect(wire::text::miniMessage::parse(std::string{reason}));
        }
    }

    /// The server's own groups, so a plugin does not carry a second permission system.
    [[nodiscard]] bool may(std::string_view permission) const override {
        const player::PlayerSession* player = session();
        if (player == nullptr || !hooks_.groups) {
            return false;
        }
        return hooks_.groups().resolve(player->group, permission);
    }

    bool give(std::string_view item, std::int32_t count) override {
        player::PlayerSession* player = session();
        if (player == nullptr || count <= 0) {
            return false;
        }
        const std::int32_t left =
            player->inventory.add(storage::StoredItemStack{.id = std::string{item}, .count = count});
        if (player->connection().sendInventoryAll) {
            player->connection().sendInventoryAll();
        }
        return left == 0;
    }

    [[nodiscard]] std::optional<std::string> heldItem() const override {
        const player::PlayerSession* player = session();
        if (player == nullptr) {
            return std::nullopt;
        }
        const std::optional<storage::StoredItemStack>& held = player->inventory.heldItem();
        return held.has_value() ? std::optional{held->id} : std::nullopt;
    }

    [[nodiscard]] PluginInventory& inventory() override { return inventory_; }

    void title(std::string_view title, std::string_view subtitle, std::int32_t fadeIn, std::int32_t stay,
               std::int32_t fadeOut) override {
        player::PlayerSession* player = session();
        if (player == nullptr || !player->connection().sendTitle) {
            return;
        }
        player->connection().sendTitle(wire::text::miniMessage::parse(std::string{title}),
                                       subtitle.empty() ? wire::text::Component{}
                                                        : wire::text::miniMessage::parse(std::string{subtitle}),
                                       fadeIn, stay, fadeOut);
    }

    void sound(std::string_view sound, float volume, float pitch) override {
        player::PlayerSession* player = session();
        if (player == nullptr || !player->connection().sendPlayPacket) {
            return;
        }
        const play::SoundPacket packet{.sound = std::string{sound},
                                       .source = play::SoundSource::Master,
                                       .x = player->x,
                                       .y = player->y,
                                       .z = player->z,
                                       .volume = volume,
                                       .pitch = pitch,
                                       .seed = 0};
        player->connection().sendPlayPacket(play::SoundPacket::kId,
                                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }

    void particle(std::string_view particle, double x, double y, double z, std::int32_t count, double spread) override {
        player::PlayerSession* player = session();
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{particle});
        const reg::ParticleType* type = id.has_value() ? reg::particleTypes::of(*id) : nullptr;
        if (player == nullptr || type == nullptr || !player->connection().sendPlayPacket) {
            return;
        }
        const play::LevelParticlesPacket packet{.particleTypeId = type->protocolId,
                                                .x = x,
                                                .y = y,
                                                .z = z,
                                                .offsetX = static_cast<float>(spread),
                                                .offsetY = static_cast<float>(spread),
                                                .offsetZ = static_cast<float>(spread),
                                                .count = count};
        player->connection().sendPlayPacket(play::LevelParticlesPacket::kId,
                                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }

    [[nodiscard]] std::optional<double> attribute(std::string_view name) const override {
        const player::PlayerSession* player = session();
        if (player == nullptr || !reg::attributeIds::exists(name)) {
            return std::nullopt;
        }
        const std::string full = qualified(name);
        const auto found = player->attributeBases.find(full);
        if (found != player->attributeBases.end()) {
            return found->second;
        }
        // Not set by anybody, so the vanilla default for a player: the only one this build keeps its
        // own number for is health, and saying 0 for the rest would be a lie.
        return full == kMaxHealthId ? std::optional{static_cast<double>(player->maxHealth())} : std::nullopt;
    }

    bool setAttribute(std::string_view name, double value) override {
        player::PlayerSession* player = session();
        if (player == nullptr || !reg::attributeIds::exists(name)) {
            return false;
        }
        const std::string full = qualified(name);
        player->attributeBases.insert_or_assign(full, value);
        // The one this server acts on itself. The rest are sent and the client honours what it can.
        if (full == kMaxHealthId) {
            player->setMaxHealth(static_cast<float>(value));
            if (player->connection().syncHealth) {
                player->connection().syncHealth();
            }
        }
        return true;
    }

    [[nodiscard]] std::vector<std::string> attributes() const override {
        std::vector<std::string> names;
        for (const std::string_view name : reg::attributeIds::all()) {
            names.emplace_back(name);
        }
        return names;
    }

private:
    [[nodiscard]] player::PlayerSession* session() const {
        for (player::PlayerSession* player : registry_->snapshot()) {
            if (player->uuid().toString() == uuid_) {
                return player;
            }
        }
        return nullptr;
    }

    player::PlayerRegistry* registry_;
    std::string uuid_;
    std::string fallbackName_;
    ModelHooks hooks_;
    SessionInventoryRef inventory_;
};

} // namespace

std::shared_ptr<PluginPlayerRef> playerRefFor(player::PlayerRegistry& registry, std::string uuid, std::string name,
                                              ModelHooks hooks) {
    return std::make_shared<SessionPlayerRef>(registry, std::move(uuid), std::move(name), std::move(hooks));
}

std::vector<std::shared_ptr<PluginPlayerRef>> RegistryPlayers::online() const {
    std::vector<std::shared_ptr<PluginPlayerRef>> refs;
    for (const player::PlayerSession* player : registry_->snapshot()) {
        refs.push_back(playerRefFor(*registry_, player->uuid().toString(), player->name(), hooks_));
    }
    return refs;
}

std::shared_ptr<PluginPlayerRef> RegistryPlayers::byName(std::string_view name) const {
    for (const player::PlayerSession* player : registry_->snapshot()) {
        if (player->name().size() == name.size() && std::ranges::equal(player->name(), name, [](char left, char right) {
                return std::tolower(left) == std::tolower(right);
            })) {
            return playerRefFor(*registry_, player->uuid().toString(), player->name(), hooks_);
        }
    }
    return nullptr;
}

std::shared_ptr<PluginPlayerRef> RegistryPlayers::byUuid(std::string_view uuid) const {
    for (const player::PlayerSession* player : registry_->snapshot()) {
        if (player->uuid().toString() == uuid) {
            return playerRefFor(*registry_, player->uuid().toString(), player->name(), hooks_);
        }
    }
    return nullptr;
}

player::PlayerSession* SessionMenus::sessionOf(const server::plugins::PluginPlayerRef& player) const {
    const std::string uuid = player.uuid();
    for (player::PlayerSession* session : registry_->snapshot()) {
        if (session->uuid().toString() == uuid) {
            return session;
        }
    }
    return nullptr;
}

server::plugins::PluginHandle SessionMenus::open(server::plugins::PluginPlayerRef& player, std::string_view title,
                                                 std::int32_t rows, const std::map<std::int32_t, PluginItem>& items,
                                                 std::function<void(std::int32_t)> onClick) {
    player::PlayerSession* session = sessionOf(player);
    if (session == nullptr || !session->connection().openPluginMenu) {
        return server::plugins::PluginHandle{};
    }
    session->connection().openPluginMenu(std::string{title}, rows, items, std::move(onClick));
    const std::string uuid = player.uuid();
    player::PlayerRegistry* registry = registry_;
    return server::plugins::PluginHandle{[registry, uuid] {
        for (player::PlayerSession* session : registry->snapshot()) {
            if (session->uuid().toString() == uuid && session->connection().closeOpenMenu) {
                session->connection().closeOpenMenu();
            }
        }
    }};
}

void SessionMenus::close(server::plugins::PluginPlayerRef& player) {
    player::PlayerSession* session = sessionOf(player);
    if (session != nullptr && session->connection().closeOpenMenu) {
        session->connection().closeOpenMenu();
    }
}

void SessionMenus::refresh(server::plugins::PluginPlayerRef& player, const std::map<std::int32_t, PluginItem>& items) {
    player::PlayerSession* session = sessionOf(player);
    if (session != nullptr && session->connection().refreshPluginMenu) {
        session->connection().refreshPluginMenu(items);
    }
}

} // namespace basalt::server::runtime::plugins
