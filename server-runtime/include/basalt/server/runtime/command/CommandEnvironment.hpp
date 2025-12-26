#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/config/CommandsConfig.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/common/timings/Timings.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

/// Declared, not defined: the plugin host is not ported yet, and a command that fires an event
/// arrives with it. Until then this is a pointer nothing dereferences.
namespace basalt::server::plugins {
class PluginEventBus;
} // namespace basalt::server::plugins

namespace basalt::server::runtime::command {

/// The clock update a world would send right now, for a join or a change.
[[nodiscard]] wire::protocol::play::SetTimePacket timePacketOf(const world::LoadedWorld& world);

/// Everything commands are allowed to reach, handed to them rather than looked up.
///
/// This is the "no `Server.getInstance()`" rule in practice: a command gets the player list, the
/// operator list and the few server-wide facts it needs through this object, so the same dispatcher
/// runs unchanged against a test fixture. Nothing here assumes a single tick thread either, which
/// is what lets a command run on whichever owner holds what it touches.
class CommandEnvironment {
public:
    /// What a `#tag` in a predicate resolves to, or nothing for a tag nobody declared.
    ///
    /// A function rather than the tag table itself: the table is loaded from the operator's own
    /// server jar and is absent when that download has not happened, and a command should then fail
    /// on the tag by name rather than on a missing service.
    using TagMembers =
        std::function<std::optional<std::set<wire::common::Identifier>>(const wire::common::Identifier& tag)>;

    /// What a caller hands over. A struct because there are twenty of them and almost all have a
    /// sensible default: a test fixture sets two.
    struct Options {
        /// Read through a supplier, not stored: `/reload` swaps the groups under a running server
        /// and a captured copy would keep answering with the permissions the server started with.
        std::function<const config::GroupsConfig&()> groups;
        /// `commands.toml`: which permission each command asks for, which exist, and what they
        /// answer.
        std::function<const config::CommandsConfig&()> commands;
        /// Where plugin events go, or null when there are no plugins. Read through a supplier: the
        /// plugin host is built after the command tree, which it registers `/plugins` into.
        std::function<server::plugins::PluginEventBus*()> pluginEvents;

        /// What `/weather` samples a duration from when it was given none, which is the level's own
        /// random in vanilla. Null where there is none, and the shortest the range allows stands in.
        random::RandomSource* random = nullptr;

        std::int64_t worldSeed = 0;
        /// The world a freshly imported or freshly joined player belongs to.
        std::string defaultWorldName = "world";
        std::int32_t maxPlayers = 20;
        wire::common::Logger logger{"Command"};
        std::function<void()> stopServer;
        /// What the console reads messages with. Clients resolve translation keys themselves
        /// against their own language, so this exists only so the log shows sentences instead of
        /// keys; an empty table degrades to printing the keys rather than failing.
        wire::text::LangTable lang;
        /// Persisted players, as opposed to who is online right now. Null where there is no
        /// database to administer, which is how the dispatcher's own tests run.
        storage::PlayerRepository* playerRepository = nullptr;
        /// Bans and the whitelist, or null where there is no database to hold them.
        storage::AccessRepository* access = nullptr;
        /// Every loaded world by name, for the commands that move players between them.
        std::map<std::string, world::LoadedWorld*, std::less<>> worlds;
        /// Every placeholder this server answers, so commands can resolve text the same way files
        /// do.
        const wire::common::placeholder::PlaceholderRegistry* placeholders = nullptr;
        /// Where a command's own cost is recorded, for `/timings`. Off unless switched on.
        wire::common::Timings* timings = nullptr;

        TagMembers blockTagMembers;
        TagMembers itemTagMembers;
        TagMembers entityTagMembers;

        /// What a selector's `scores=`, `team=` and `advancements=` read.
        ///
        /// Null rather than empty: without them the filter has nothing to answer with and matches
        /// nobody, which is also what vanilla does for an objective or advancement that does not
        /// exist, so a test fixture with no scoreboard behaves like a server with an empty one.
        scoreboard::Scoreboard* scoreboard = nullptr;
        scoreboard::Teams* teams = nullptr;
        /// What `/time query <timeline>` reads a period from, or null where no jar is loaded.
        const world::RealTimelines* timelines = nullptr;
        const world::RealAdvancements* advancements = nullptr;
        world::PlayerProgress* progress = nullptr;
        /// What a command hands a criterion to, so `/recipe give` awards what learning it would.
        world::AdvancementTriggers* advancementTriggers = nullptr;
    };

    CommandEnvironment(player::PlayerRegistry& players, Options options);

    [[nodiscard]] player::PlayerRegistry& players() const noexcept { return *players_; }

    [[nodiscard]] const Options& options() const noexcept { return options_; }

    [[nodiscard]] const wire::common::Logger& logger() const noexcept { return options_.logger; }

    [[nodiscard]] const wire::text::LangTable& lang() const noexcept { return options_.lang; }

    [[nodiscard]] const config::GroupsConfig& groups() const;

    [[nodiscard]] const config::CommandsConfig& commands() const;

    /// Where plugin events go right now, or null when there are none.
    [[nodiscard]] server::plugins::PluginEventBus* pluginEvents() const;

    [[nodiscard]] const wire::common::placeholder::PlaceholderRegistry& placeholders() const;

    /// A message every connected player sees, plus the console, the way vanilla's
    /// `broadcastSystemMessage` does.
    void broadcast(const wire::text::Component& message) const;

    /// Everyone standing in one world, for the packets that only mean anything to them.
    [[nodiscard]] std::vector<player::PlayerSession*> playersIn(const world::LoadedWorld& world) const;

    /// What a block a command changed asked everyone in that world to hear: a door, a note, a bell.
    void blockEffects(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                      std::int32_t stateId, const std::vector<world::BlockEffect>& effects) const;

    /// Pushes a world's clock to whoever is in it, after a `/time` moved it.
    void syncTime(const world::LoadedWorld& world) const;

    /// Re-sends every chunk in a block range to whoever is in the world: for changes too big to
    /// patch.
    void syncChunks(const world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                    std::int32_t maxZ) const;

    /// Pushes new biomes for a block range to whoever is in the world, without resending the
    /// columns.
    void syncBiomes(const world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                    std::int32_t maxZ) const;

    /// Pushes a world's weather, which is three game events rather than one packet.
    void syncWeather(const world::LoadedWorld& world) const;

private:
    player::PlayerRegistry* players_;
    Options options_;
    config::GroupsConfig emptyGroups_;
    config::CommandsConfig emptyCommands_;
    wire::common::placeholder::PlaceholderRegistry ownPlaceholders_;
};

} // namespace basalt::server::runtime::command
