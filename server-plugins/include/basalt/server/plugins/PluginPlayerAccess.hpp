#pragma once

#include <basalt/server/plugins/PluginEntityOps.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
class PluginWindow;
class PluginWorldRef;
class PluginWorldEnv;

struct PluginItem {
    std::string id;
    std::int32_t count = 1;
    std::optional<std::string> name;
    std::vector<std::string> lore;
    std::int32_t damage = 0;
    std::map<std::string, std::int32_t> enchantments;
    wire::nbt::NbtCompound components;

    friend bool operator==(const PluginItem&, const PluginItem&) = default;
};

class PluginInventory {
public:
    PluginInventory() = default;
    PluginInventory(const PluginInventory&) = delete;
    PluginInventory& operator=(const PluginInventory&) = delete;
    PluginInventory(PluginInventory&&) = delete;
    PluginInventory& operator=(PluginInventory&&) = delete;
    virtual ~PluginInventory() = default;

    [[nodiscard]] virtual std::int32_t size() const = 0;

    [[nodiscard]] virtual std::int32_t heldSlot() const = 0;

    [[nodiscard]] virtual std::optional<PluginItem> at(std::int32_t slot) const = 0;

    virtual void set(std::int32_t slot, const std::optional<PluginItem>& item) = 0;

    virtual std::int32_t add(const PluginItem& item) = 0;

    virtual std::int32_t remove(std::string_view id, std::int32_t count) = 0;

    [[nodiscard]] virtual std::int32_t count(std::string_view id) const = 0;

    virtual void clear() = 0;

    virtual void setHeldSlot(std::int32_t slot) { static_cast<void>(slot); }
};

class PluginPlayerRef : public std::enable_shared_from_this<PluginPlayerRef> {
public:
    PluginPlayerRef() = default;
    PluginPlayerRef(const PluginPlayerRef&) = delete;
    PluginPlayerRef& operator=(const PluginPlayerRef&) = delete;
    PluginPlayerRef(PluginPlayerRef&&) = delete;
    PluginPlayerRef& operator=(PluginPlayerRef&&) = delete;
    virtual ~PluginPlayerRef() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string uuid() const = 0;
    [[nodiscard]] virtual bool online() const = 0;

    [[nodiscard]] virtual double health() const = 0;
    virtual void setHealth(double value) = 0;
    [[nodiscard]] virtual std::int32_t food() const = 0;
    virtual void setFood(std::int32_t value) = 0;
    [[nodiscard]] virtual std::string gamemode() const = 0;
    virtual void setGamemode(std::string_view value) = 0;
    [[nodiscard]] virtual std::int32_t level() const = 0;
    virtual void setLevel(std::int32_t value) = 0;

    [[nodiscard]] virtual std::string world() const = 0;
    [[nodiscard]] virtual double x() const = 0;
    [[nodiscard]] virtual double y() const = 0;
    [[nodiscard]] virtual double z() const = 0;
    [[nodiscard]] virtual std::int32_t ping() const = 0;
    [[nodiscard]] virtual bool sneaking() const = 0;

    virtual void send(std::string_view message) = 0;

    virtual void actionBar(std::string_view message) = 0;

    virtual bool teleport(std::string_view world, double x, double y, double z) = 0;

    virtual void kick(std::string_view reason) = 0;

    [[nodiscard]] virtual bool may(std::string_view permission) const = 0;

    /// The profile properties the player logged in with (their skin), as name, value and signature triples.
    [[nodiscard]] virtual std::vector<std::array<std::string, 3>> profileProperties() const { return {}; }

    /// What `source` (a plugin) grants or denies this player over their group; nothing withdraws it.
    virtual void setPermission(std::string_view source, std::string_view node, std::optional<bool> granted) {
        static_cast<void>(source);
        static_cast<void>(node);
        static_cast<void>(granted);
    }

    /// Replaces everything `source` grants or denies this player with these nodes.
    virtual void setPermissions(std::string_view source, const std::vector<std::pair<std::string, bool>>& nodes) {
        static_cast<void>(source);
        static_cast<void>(nodes);
    }

    virtual bool give(std::string_view item, std::int32_t count) = 0;

    [[nodiscard]] virtual std::optional<std::string> heldItem() const = 0;

    [[nodiscard]] virtual PluginInventory& inventory() = 0;

    /// The container window this player has open, or null when the connection has no windows (Bedrock).
    [[nodiscard]] virtual PluginWindow* window() { return nullptr; }

    virtual void title(std::string_view title, std::string_view subtitle, std::int32_t fadeIn, std::int32_t stay,
                       std::int32_t fadeOut) = 0;

    virtual void sound(std::string_view sound, float volume, float pitch) = 0;

    virtual void particle(std::string_view particle, double x, double y, double z, std::int32_t count,
                          double spread) = 0;

    virtual bool effect(std::string_view effect, std::int32_t durationTicks, std::int32_t amplifier) = 0;

    [[nodiscard]] virtual std::optional<double> attribute(std::string_view name) const = 0;

    virtual bool setAttribute(std::string_view name, double value) = 0;

    [[nodiscard]] virtual std::vector<std::string> attributes() const = 0;

    [[nodiscard]] virtual float yaw() const { return 0.0F; }

    [[nodiscard]] virtual float pitch() const { return 0.0F; }

    virtual bool teleport(std::string_view world, double x, double y, double z, float yaw, float pitch) {
        static_cast<void>(yaw);
        static_cast<void>(pitch);
        return teleport(world, x, y, z);
    }

    /// The remote address as `host:port`, empty when unknown.
    [[nodiscard]] virtual std::string address() const { return {}; }

    /// The command permission level, 0 to 4.
    [[nodiscard]] virtual std::int32_t permissionLevel() const { return 0; }

    [[nodiscard]] virtual bool flying() const { return false; }

    [[nodiscard]] virtual bool onGround() const { return false; }

    [[nodiscard]] virtual std::int32_t entityId() const { return 0; }

    /// Tells this player alone that a block looks different, without changing the world.
    virtual void showBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(stateId);
    }

    virtual void setExperience(float progress, std::int32_t level) {
        static_cast<void>(progress);
        setLevel(level);
    }

    [[nodiscard]] virtual float experienceProgress() const { return 0.0F; }

    [[nodiscard]] virtual std::int32_t experiencePoints() const { return 0; }

    [[nodiscard]] virtual float saturation() const { return 0.0F; }

    virtual void setHeldSlot(std::int32_t slot) { static_cast<void>(slot); }

    [[nodiscard]] virtual bool sprinting() const { return false; }

    /// Player state and operations by name (see `PluginPlayerOps.hpp`); nothing means the name is unknown.
    [[nodiscard]] virtual std::optional<PluginEntityValue> property(std::string_view name) const {
        static_cast<void>(name);
        return std::nullopt;
    }

    virtual bool setProperty(std::string_view name, const PluginEntityValue& value) {
        static_cast<void>(name);
        static_cast<void>(value);
        return false;
    }

    virtual std::optional<PluginEntityValue> invoke(std::string_view operation,
                                                    const std::vector<PluginEntityValue>& arguments) {
        static_cast<void>(operation);
        static_cast<void>(arguments);
        return std::nullopt;
    }
};

class PluginEntityRef {
public:
    PluginEntityRef() = default;
    PluginEntityRef(const PluginEntityRef&) = delete;
    PluginEntityRef& operator=(const PluginEntityRef&) = delete;
    PluginEntityRef(PluginEntityRef&&) = delete;
    PluginEntityRef& operator=(PluginEntityRef&&) = delete;
    virtual ~PluginEntityRef() = default;

    [[nodiscard]] virtual std::int32_t id() const = 0;
    [[nodiscard]] virtual std::string type() const = 0;
    [[nodiscard]] virtual std::string world() const = 0;
    [[nodiscard]] virtual double x() const = 0;
    [[nodiscard]] virtual double y() const = 0;
    [[nodiscard]] virtual double z() const = 0;
    [[nodiscard]] virtual double health() const = 0;
    virtual void setHealth(double value) = 0;
    [[nodiscard]] virtual bool alive() const = 0;

    virtual void remove() = 0;

    virtual bool teleport(double x, double y, double z) = 0;

    [[nodiscard]] virtual std::string uuid() const { return {}; }

    [[nodiscard]] virtual std::array<float, 2> rotation() const { return {0.0F, 0.0F}; }

    virtual bool setRotation(float yaw, float pitch) {
        static_cast<void>(yaw);
        static_cast<void>(pitch);
        return false;
    }

    [[nodiscard]] virtual std::array<double, 3> velocity() const { return {0.0, 0.0, 0.0}; }

    virtual bool setVelocity(double x, double y, double z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return false;
    }

    [[nodiscard]] virtual bool onGround() const { return false; }

    [[nodiscard]] virtual std::int32_t age() const { return 0; }

    [[nodiscard]] virtual double width() const { return 0.6; }

    [[nodiscard]] virtual double height() const { return 1.8; }

    [[nodiscard]] virtual std::optional<std::string> customName() const { return std::nullopt; }

    /// A named live property, see `kEntityProperties`; nullopt when the entity has no such thing.
    [[nodiscard]] virtual std::optional<PluginEntityValue> property(std::string_view name) const {
        static_cast<void>(name);
        return std::nullopt;
    }

    virtual bool setProperty(std::string_view name, const PluginEntityValue& value) {
        static_cast<void>(name);
        static_cast<void>(value);
        return false;
    }

    /// A named operation, see `kEntityOperations`; nullopt when the entity cannot do it.
    virtual std::optional<PluginEntityValue> invoke(std::string_view operation,
                                                    const std::vector<PluginEntityValue>& arguments) {
        static_cast<void>(operation);
        static_cast<void>(arguments);
        return std::nullopt;
    }
};

class PluginHologramRef : public PluginEntityRef {
public:
    [[nodiscard]] virtual std::string text() const = 0;
    virtual void setText(std::string_view value) = 0;

    virtual void showOnlyTo(const std::vector<std::string>& uuids) = 0;

    virtual void showToAll() = 0;

    [[nodiscard]] virtual std::optional<std::vector<std::string>> audience() const = 0;
};

class PluginNpcRef {
public:
    PluginNpcRef() = default;
    PluginNpcRef(const PluginNpcRef&) = delete;
    PluginNpcRef& operator=(const PluginNpcRef&) = delete;
    PluginNpcRef(PluginNpcRef&&) = delete;
    PluginNpcRef& operator=(PluginNpcRef&&) = delete;
    virtual ~PluginNpcRef() = default;

    [[nodiscard]] virtual std::int32_t id() const = 0;
    [[nodiscard]] virtual std::string uuid() const = 0;
    [[nodiscard]] virtual std::string world() const = 0;
    [[nodiscard]] virtual double x() const = 0;
    [[nodiscard]] virtual double y() const = 0;
    [[nodiscard]] virtual double z() const = 0;
    [[nodiscard]] virtual bool alive() const = 0;

    [[nodiscard]] virtual std::string name() const = 0;
    virtual void setName(std::string_view value) = 0;

    virtual void skin(std::string_view texture, const std::optional<std::string>& signature) = 0;

    virtual bool skinFrom(std::string_view playerName) = 0;

    virtual void look(float yaw, float pitch) = 0;

    virtual void looksAtPlayers(bool follows) = 0;

    virtual bool teleport(double x, double y, double z) = 0;

    virtual void showOnlyTo(const std::vector<std::string>& uuids) = 0;

    virtual void showToAll() = 0;

    [[nodiscard]] virtual std::optional<std::vector<std::string>> audience() const = 0;

    virtual void remove() = 0;
};

class PluginPlayers {
public:
    PluginPlayers() = default;
    PluginPlayers(const PluginPlayers&) = delete;
    PluginPlayers& operator=(const PluginPlayers&) = delete;
    PluginPlayers(PluginPlayers&&) = delete;
    PluginPlayers& operator=(PluginPlayers&&) = delete;
    virtual ~PluginPlayers() = default;

    [[nodiscard]] virtual std::vector<std::shared_ptr<PluginPlayerRef>> online() const = 0;

    [[nodiscard]] virtual std::shared_ptr<PluginPlayerRef> byName(std::string_view name) const = 0;

    [[nodiscard]] virtual std::shared_ptr<PluginPlayerRef> byUuid(std::string_view uuid) const = 0;
};
} // namespace basalt::server::plugins
