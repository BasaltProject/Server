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

class RealAdvancements {
public:
    struct Jar {
        std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
        std::function<std::optional<std::string>(std::string_view name)> readText;
    };

    explicit RealAdvancements(std::map<wire::common::Identifier, server::world::advancement::Advancement> definitions);

    explicit RealAdvancements(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Advancements"});

    [[nodiscard]] std::size_t size() const noexcept { return advancements_.size(); }

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    [[nodiscard]] const server::world::advancement::Advancement* find(const wire::common::Identifier& id) const;

    [[nodiscard]] std::vector<wire::common::Identifier> descendantsOf(const wire::common::Identifier& id) const;

    [[nodiscard]] std::vector<wire::common::Identifier> ancestorsOf(const wire::common::Identifier& id) const;

private:
    std::map<wire::common::Identifier, server::world::advancement::Advancement> advancements_;
};

/// Keyed by UUID as vanilla keys `advancements/<uuid>.json`; with no repository nothing is persisted.
class PlayerProgress {
public:
    PlayerProgress() = default;

    explicit PlayerProgress(storage::ProgressRepository* repository) : repository_(repository) {}

    void load(std::string_view uuid);

    void unload(std::string_view uuid);

    using Changed = std::function<void(std::string_view uuid, const wire::common::Identifier& advancement)>;

    void onChanged(Changed changed) { changed_ = std::move(changed); }

    bool grantCriterion(std::string_view uuid, const wire::common::Identifier& advancement, std::string_view criterion);

    /// Epoch milliseconds.
    [[nodiscard]] std::map<std::string, std::int64_t, std::less<>>
    obtained(std::string_view uuid, const wire::common::Identifier& advancement) const;

    [[nodiscard]] std::vector<wire::common::Identifier> started(std::string_view uuid) const;

    bool revokeCriterion(std::string_view uuid, const wire::common::Identifier& advancement,
                         std::string_view criterion);

    [[nodiscard]] std::set<std::string, std::less<>> grantedCriteria(std::string_view uuid,
                                                                     const wire::common::Identifier& advancement) const;

    bool revokeAll(std::string_view uuid, const wire::common::Identifier& advancement);

    [[nodiscard]] std::set<wire::common::Identifier> knownRecipes(std::string_view uuid) const;

    bool learn(std::string_view uuid, const wire::common::Identifier& recipe);

    bool forget(std::string_view uuid, const wire::common::Identifier& recipe);

    [[nodiscard]] std::set<wire::common::Identifier> highlightedRecipes(std::string_view uuid) const;

    /// parity: `ServerRecipeBook.removeHighlight`.
    void removeHighlight(std::string_view uuid, const wire::common::Identifier& recipe);

    /// open and filtering per RecipeBookType ordinal; absent means both off.
    [[nodiscard]] std::map<std::int32_t, std::pair<bool, bool>> bookSettings(std::string_view uuid) const;

    void setBookSetting(std::string_view uuid, std::int32_t book, bool open, bool filtering);

private:
    storage::ProgressRepository* repository_ = nullptr;
    std::map<std::string, std::map<wire::common::Identifier, std::map<std::string, std::int64_t, std::less<>>>,
             std::less<>>
        advancements_;
    Changed changed_;
    std::map<std::string, std::set<wire::common::Identifier>, std::less<>> recipes_;
    std::map<std::string, std::set<wire::common::Identifier>, std::less<>> highlighted_;
    std::map<std::string, std::map<std::int32_t, std::pair<bool, bool>>, std::less<>> bookSettings_;
};

} // namespace basalt::server::runtime::world
