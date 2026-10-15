#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::plugins {
struct PluginItem {
    std::string id;
    std::int32_t count = 1;
    std::optional<std::string> name;
    std::vector<std::string> lore;
    std::int32_t damage = 0;
    std::map<std::string, std::int32_t> enchantments;

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

    virtual bool give(std::string_view item, std::int32_t count) = 0;

    [[nodiscard]] virtual std::optional<std::string> heldItem() const = 0;

    [[nodiscard]] virtual PluginInventory& inventory() = 0;

    virtual void title(std::string_view title, std::string_view subtitle, std::int32_t fadeIn, std::int32_t stay,
                       std::int32_t fadeOut) = 0;

    virtual void sound(std::string_view sound, float volume, float pitch) = 0;

    virtual void particle(std::string_view particle, double x, double y, double z, std::int32_t count,
                          double spread) = 0;

    virtual bool effect(std::string_view effect, std::int32_t durationTicks, std::int32_t amplifier) = 0;

    [[nodiscard]] virtual std::optional<double> attribute(std::string_view name) const = 0;

    virtual bool setAttribute(std::string_view name, double value) = 0;

    [[nodiscard]] virtual std::vector<std::string> attributes() const = 0;
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

class PluginWorldRef {
public:
    PluginWorldRef() = default;
    PluginWorldRef(const PluginWorldRef&) = delete;
    PluginWorldRef& operator=(const PluginWorldRef&) = delete;
    PluginWorldRef(PluginWorldRef&&) = delete;
    PluginWorldRef& operator=(PluginWorldRef&&) = delete;
    virtual ~PluginWorldRef() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    [[nodiscard]] virtual std::int64_t time() const = 0;

    [[nodiscard]] virtual bool raining() const = 0;

    [[nodiscard]] virtual bool thundering() const = 0;

    [[nodiscard]] virtual std::string difficulty() const = 0;

    [[nodiscard]] virtual std::optional<std::string> block(std::int32_t x, std::int32_t y, std::int32_t z) const = 0;

    virtual bool setBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view block) = 0;

    [[nodiscard]] virtual std::vector<std::shared_ptr<PluginEntityRef>> entities() const = 0;

    virtual std::shared_ptr<PluginHologramRef> hologram(std::string_view text, double x, double y, double z) = 0;

    virtual std::shared_ptr<PluginNpcRef> npc(std::string_view name, double x, double y, double z,
                                              std::function<void(const std::string&)> onClick) = 0;

    virtual std::shared_ptr<PluginEntityRef> spawn(std::string_view type, double x, double y, double z) = 0;

    [[nodiscard]] virtual std::optional<std::string> gameRule(std::string_view name) const = 0;

    virtual bool setGameRule(std::string_view name, std::string_view value) = 0;

    [[nodiscard]] virtual std::vector<std::string> gameRules() const = 0;
};

class PluginWorlds {
public:
    PluginWorlds() = default;
    PluginWorlds(const PluginWorlds&) = delete;
    PluginWorlds& operator=(const PluginWorlds&) = delete;
    PluginWorlds(PluginWorlds&&) = delete;
    PluginWorlds& operator=(PluginWorlds&&) = delete;
    virtual ~PluginWorlds() = default;

    [[nodiscard]] virtual std::vector<std::shared_ptr<PluginWorldRef>> all() const = 0;

    [[nodiscard]] virtual std::shared_ptr<PluginWorldRef> byName(std::string_view name) const = 0;

    [[nodiscard]] virtual std::shared_ptr<PluginWorldRef> defaultWorld() const = 0;

    virtual std::shared_ptr<PluginWorldRef> create(std::string_view name, std::string_view dimension,
                                                   std::string_view type, std::optional<std::int64_t> seed) = 0;
};

class PluginPlaceholders {
public:
    using Value = std::function<std::string(PluginPlayerRef*)>;

    PluginPlaceholders() = default;
    PluginPlaceholders(const PluginPlaceholders&) = delete;
    PluginPlaceholders& operator=(const PluginPlaceholders&) = delete;
    PluginPlaceholders(PluginPlaceholders&&) = delete;
    PluginPlaceholders& operator=(PluginPlaceholders&&) = delete;
    virtual ~PluginPlaceholders() = default;

    virtual PluginHandle registerPlaceholder(std::string_view pluginId, std::string_view name, Value value) = 0;

    [[nodiscard]] virtual std::string fill(std::string_view text, PluginPlayerRef* player) const = 0;
};
} // namespace basalt::server::plugins
