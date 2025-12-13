#pragma once

#include <basalt/server/storage/ProgressRepository.hpp>
#include <basalt/server/world/advancement/Advancement.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// Every advancement the operator's own `server.jar` ships, by id.
///
/// Datapack content, so it is read from the download rather than committed, the same way loot
/// tables and recipes are. What awards them is `AdvancementTriggers`; `/advancement grant` remains
/// the only way to complete one whose trigger this build cannot observe yet.
class RealAdvancements {
public:
    /// The jar's directory listing and its file reader, as callbacks rather than the jar itself.
    struct Jar {
        std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
        std::function<std::optional<std::string>(std::string_view name)> readText;
    };

    /// Definitions handed in directly, which is how a test builds one without a jar to read.
    explicit RealAdvancements(std::map<wire::common::Identifier, server::world::advancement::Advancement> definitions);

    explicit RealAdvancements(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Advancements"});

    [[nodiscard]] std::size_t size() const noexcept { return advancements_.size(); }

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    [[nodiscard]] const server::world::advancement::Advancement* find(const wire::common::Identifier& id) const;

    /// This one and everything below it, which is what `/advancement grant ... from` acts on.
    [[nodiscard]] std::vector<wire::common::Identifier> descendantsOf(const wire::common::Identifier& id) const;

    /// Everything from the root down to this one, which is `/advancement grant ... until`.
    [[nodiscard]] std::vector<wire::common::Identifier> ancestorsOf(const wire::common::Identifier& id) const;

private:
    std::map<wire::common::Identifier, server::world::advancement::Advancement> advancements_;
};

/// What each player has been granted or taught, kept in memory and written through to `players.db`.
///
/// Keyed by UUID rather than by name, which is both what the database stores and what survives a
/// rename: vanilla keys `advancements/<uuid>.json` the same way. `load` fills a player in at join;
/// everything else answers from memory, so a lookup on the crafting path costs no query.
///
/// The repository is unset where there is no database, which is how the command tests run: the
/// whole class then behaves as it did before persistence existed.
class PlayerProgress {
public:
    PlayerProgress() = default;

    explicit PlayerProgress(storage::ProgressRepository* repository) : repository_(repository) {}

    /// Reads one player's rows in. Called at join, before anything can ask about them.
    void load(std::string_view uuid);

    /// Drops a player from memory at quit. Their rows stay in the database.
    void unload(std::string_view uuid);

    /// Marks one criterion as done. Answers false when it already was.
    bool grantCriterion(std::string_view uuid, const wire::common::Identifier& advancement, std::string_view criterion);

    bool revokeCriterion(std::string_view uuid, const wire::common::Identifier& advancement,
                         std::string_view criterion);

    [[nodiscard]] std::set<std::string, std::less<>> grantedCriteria(std::string_view uuid,
                                                                     const wire::common::Identifier& advancement) const;

    bool revokeAll(std::string_view uuid, const wire::common::Identifier& advancement);

    [[nodiscard]] std::set<wire::common::Identifier> knownRecipes(std::string_view uuid) const;

    bool learn(std::string_view uuid, const wire::common::Identifier& recipe);

    bool forget(std::string_view uuid, const wire::common::Identifier& recipe);

private:
    storage::ProgressRepository* repository_ = nullptr;
    std::map<std::string, std::map<wire::common::Identifier, std::set<std::string, std::less<>>>, std::less<>>
        advancements_;
    std::map<std::string, std::set<wire::common::Identifier>, std::less<>> recipes_;
};

} // namespace basalt::server::runtime::world
