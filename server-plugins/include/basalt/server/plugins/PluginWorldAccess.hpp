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

/// One stack, as a plugin sees it: a value rather than a handle.
///
/// The one place the object model goes the other way, because a stack has no identity of its own. It
/// is whatever is in a slot right now, so changing one is putting a different one in the slot.
struct PluginItem {
    std::string id;
    std::int32_t count = 1;
    /// The custom name, as MiniMessage, or nothing for whatever the item is called by default.
    std::optional<std::string> name;
    std::vector<std::string> lore;
    std::int32_t damage = 0;
    /// Enchantment id to level: `minecraft:sharpness` to 3.
    std::map<std::string, std::int32_t> enchantments;

    friend bool operator==(const PluginItem&, const PluginItem&) = default;
};

/// The slots of somebody, addressed by number.
///
/// Slot numbers are vanilla's, because they are what the client clicks and what an event carries:
/// 0 to 35 is the main inventory with the hotbar at 27 to 35, 36 to 39 is armour and 40 is the
/// offhand. Renumbering them would only mean a plugin reading one number in an event and passing a
/// different one back.
class PluginInventory {
public:
    PluginInventory() = default;
    PluginInventory(const PluginInventory&) = delete;
    PluginInventory& operator=(const PluginInventory&) = delete;
    PluginInventory(PluginInventory&&) = delete;
    PluginInventory& operator=(PluginInventory&&) = delete;
    virtual ~PluginInventory() = default;

    [[nodiscard]] virtual std::int32_t size() const = 0;

    /// Which hotbar slot is in hand, 0 to 8.
    [[nodiscard]] virtual std::int32_t heldSlot() const = 0;

    [[nodiscard]] virtual std::optional<PluginItem> at(std::int32_t slot) const = 0;

    /// Puts `item` in `slot`, or empties it with nothing.
    virtual void set(std::int32_t slot, const std::optional<PluginItem>& item) = 0;

    /// Adds it the way picking something up does. Answers how many did *not* fit.
    virtual std::int32_t add(const PluginItem& item) = 0;

    /// Takes up to `count` of `id` out. Answers how many were actually taken.
    virtual std::int32_t remove(std::string_view id, std::int32_t count) = 0;

    [[nodiscard]] virtual std::int32_t count(std::string_view id) const = 0;

    virtual void clear() = 0;
};

/// A player, as much of one as a plugin is allowed to touch.
///
/// A handle rather than a copy: reading `health` reads the player's health now, and writing it
/// changes it. The handle stops working when they leave, and says so rather than quietly writing
/// into nothing.
class PluginPlayerRef {
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

    /// MiniMessage, like every other piece of text a plugin writes.
    virtual void send(std::string_view message) = 0;

    virtual void actionBar(std::string_view message) = 0;

    virtual bool teleport(std::string_view world, double x, double y, double z) = 0;

    virtual void kick(std::string_view reason) = 0;

    /// Against the server's own groups, so a plugin does not keep a second permission system.
    [[nodiscard]] virtual bool may(std::string_view permission) const = 0;

    virtual bool give(std::string_view item, std::int32_t count) = 0;

    /// What they are holding, as `minecraft:stone`, or nothing for an empty hand.
    [[nodiscard]] virtual std::optional<std::string> heldItem() const = 0;

    /// Their 41 slots. Still answers when they are offline, by doing nothing.
    [[nodiscard]] virtual PluginInventory& inventory() = 0;

    /// The big text in the middle. Empty strings leave that half of it alone.
    virtual void title(std::string_view title, std::string_view subtitle, std::int32_t fadeIn, std::int32_t stay,
                       std::int32_t fadeOut) = 0;

    /// A sound at their own position, which is what a plugin wants nine times out of ten.
    virtual void sound(std::string_view sound, float volume, float pitch) = 0;

    virtual void particle(std::string_view particle, double x, double y, double z, std::int32_t count,
                          double spread) = 0;

    /// An attribute of theirs by its registry name, or nothing if there is no attribute by that name.
    ///
    /// The server's own, not a value drawn on the client: setting `max_health` moves what heals
    /// clamp to and what damage measures against, not only how many hearts are painted.
    [[nodiscard]] virtual std::optional<double> attribute(std::string_view name) const = 0;

    virtual bool setAttribute(std::string_view name, double value) = 0;

    [[nodiscard]] virtual std::vector<std::string> attributes() const = 0;
};

/// Anything alive that is not a player: a mob, an item on the ground, a projectile.
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

/// Floating text somebody put in the world.
///
/// An entity like any other, plus the two things a hologram is for: text that can be changed without
/// making a new one, and being shown to some people and not others.
class PluginHologramRef : public PluginEntityRef {
public:
    /// MiniMessage, changed as often as you like: the entity stays the same one.
    [[nodiscard]] virtual std::string text() const = 0;
    virtual void setText(std::string_view value) = 0;

    /// Shows it only to these players, by uuid. An empty list hides it from everybody.
    virtual void showOnlyTo(const std::vector<std::string>& uuids) = 0;

    virtual void showToAll() = 0;

    /// Who it is shown to, or nothing when that is everybody.
    [[nodiscard]] virtual std::optional<std::vector<std::string>> audience() const = 0;
};

/// A player-shaped thing a plugin put somewhere.
///
/// A real player entity, with a profile and a skin, because this is the server: there are no faked
/// packets here and nothing to detect about the client's version.
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

    /// The skin, as the value and signature of a profile property, which is how skins travel.
    virtual void skin(std::string_view texture, const std::optional<std::string>& signature) = 0;

    /// Wears the skin of a player who is connected. Answers false if nobody by that name is.
    ///
    /// The way to give an NPC a face without pasting a signed blob into a config: the textures of a
    /// real profile are already here once somebody joins, and they are what a skin is.
    virtual bool skinFrom(std::string_view playerName) = 0;

    /// Points it somewhere fixed, which also stops it following people.
    virtual void look(float yaw, float pitch) = 0;

    /// Whether its head turns toward whoever comes near, which it does unless told otherwise.
    virtual void looksAtPlayers(bool follows) = 0;

    virtual bool teleport(double x, double y, double z) = 0;

    virtual void showOnlyTo(const std::vector<std::string>& uuids) = 0;

    virtual void showToAll() = 0;

    [[nodiscard]] virtual std::optional<std::vector<std::string>> audience() const = 0;

    virtual void remove() = 0;
};

/// Who is connected, and who was.
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

/// One world, and the blocks in it.
class PluginWorldRef {
public:
    PluginWorldRef() = default;
    PluginWorldRef(const PluginWorldRef&) = delete;
    PluginWorldRef& operator=(const PluginWorldRef&) = delete;
    PluginWorldRef(PluginWorldRef&&) = delete;
    PluginWorldRef& operator=(PluginWorldRef&&) = delete;
    virtual ~PluginWorldRef() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    /// Ticks since the world began, which is what the clock and the day count are made of.
    [[nodiscard]] virtual std::int64_t time() const = 0;

    [[nodiscard]] virtual bool raining() const = 0;

    [[nodiscard]] virtual bool thundering() const = 0;

    [[nodiscard]] virtual std::string difficulty() const = 0;

    /// `minecraft:stone`, or nothing when that chunk is not loaded: a plugin should know the
    /// difference.
    [[nodiscard]] virtual std::optional<std::string> block(std::int32_t x, std::int32_t y, std::int32_t z) const = 0;

    virtual bool setBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view block) = 0;

    [[nodiscard]] virtual std::vector<std::shared_ptr<PluginEntityRef>> entities() const = 0;

    /// Floating text at a position, as a real `text_display` entity.
    ///
    /// Not an armour stand with a name and not a packet sent by hand: 26.2 has an entity for this,
    /// so a hologram is one, which is why it survives a chunk reload.
    virtual std::shared_ptr<PluginHologramRef> hologram(std::string_view text, double x, double y, double z) = 0;

    /// A player-shaped entity standing here, which is what an NPC is.
    ///
    /// `name` may carry newlines: it is drawn as a display above its head rather than as its profile
    /// name. `onClick` gets the uuid of whoever right-clicked it.
    virtual std::shared_ptr<PluginNpcRef> npc(std::string_view name, double x, double y, double z,
                                              std::function<void(const std::string&)> onClick) = 0;

    /// Spawns a mob and hands it back, or nothing if the type is not one.
    ///
    /// This goes through the same path a natural spawn does, so a plugin that refuses those refuses
    /// this too.
    virtual std::shared_ptr<PluginEntityRef> spawn(std::string_view type, double x, double y, double z) = 0;

    /// A gamerule of this world, as `/gamerule` prints it, or nothing if there is no such rule.
    ///
    /// The server's own rules, not a copy: a plugin that turns off `doDaylightCycle` turns off the
    /// one the world is ticking.
    [[nodiscard]] virtual std::optional<std::string> gameRule(std::string_view name) const = 0;

    /// Answers false if there is no such rule, or if the value does not fit its type.
    virtual bool setGameRule(std::string_view name, std::string_view value) = 0;

    [[nodiscard]] virtual std::vector<std::string> gameRules() const = 0;
};

/// The worlds this server has.
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

    /// Makes a world that was not in `worlds.toml`.
    ///
    /// It is a world like any other: its own folder, its own `settings.toml` inheriting the
    /// defaults, its own seed. Nothing if the name is taken or unusable. `dimension` is `overworld`,
    /// `nether` or `the_end`, and `type` is `normal`, `flat`, `amplified` or `void`.
    virtual std::shared_ptr<PluginWorldRef> create(std::string_view name, std::string_view dimension,
                                                   std::string_view type, std::optional<std::int64_t> seed) = 0;
};

/// What a plugin can put its own name on: placeholders the rest of the server already renders.
///
/// Registering here rather than in a registry of the plugin's own is the point: `%arena_wins%` works
/// in the MOTD, in the sidebar and in another plugin's message, because there is one registry.
class PluginPlaceholders {
public:
    /// What a placeholder answers, for whoever it is rendered against. The player is null when it is
    /// rendered without one (the server list, a console message).
    using Value = std::function<std::string(PluginPlayerRef*)>;

    PluginPlaceholders() = default;
    PluginPlaceholders(const PluginPlaceholders&) = delete;
    PluginPlaceholders& operator=(const PluginPlaceholders&) = delete;
    PluginPlaceholders(PluginPlaceholders&&) = delete;
    PluginPlaceholders& operator=(PluginPlaceholders&&) = delete;
    virtual ~PluginPlaceholders() = default;

    /// Registers `%<pluginId>_<name>%`.
    virtual PluginHandle registerPlaceholder(std::string_view pluginId, std::string_view name, Value value) = 0;

    /// Renders every placeholder in `text` for `player`, the same way the server's own files are.
    [[nodiscard]] virtual std::string fill(std::string_view text, PluginPlayerRef* player) const = 0;
};

} // namespace basalt::server::plugins
