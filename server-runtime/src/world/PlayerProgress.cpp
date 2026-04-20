#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/world/advancement/AdvancementJson.hpp>

#include <algorithm>
#include <chrono>
#include <expected>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using advancement = server::world::advancement::Advancement;

} // namespace

RealAdvancements::RealAdvancements(std::map<Identifier, advancement> definitions)
    : advancements_(std::move(definitions)) {}

RealAdvancements::RealAdvancements(const Jar& jar, wire::common::Logger logger) {
    if (!jar.entriesUnder || !jar.readText) {
        return;
    }
    for (const auto& [id, entry] : datapack::dataFiles(jar.entriesUnder, "advancement")) {
        const std::optional<std::string> text = jar.readText(entry);
        if (!text.has_value()) {
            logger.debug([&] { return "Skipped advancement " + entry + ": unreadable"; });
            continue;
        }
        const std::expected<advancement, server::world::advancement::AdvancementParseError> parsed =
            server::world::advancement::advancementJson::parse(*text);
        if (!parsed.has_value()) {
            logger.error([&] { return "Couldn't parse advancement " + id.toString(); });
            continue;
        }
        advancements_.insert_or_assign(id, *parsed);
    }
    logger.info([this] { return "Loaded " + std::to_string(advancements_.size()) + " advancement(s)."; });
}

std::vector<Identifier> RealAdvancements::ids() const {
    std::vector<Identifier> names;
    names.reserve(advancements_.size());
    for (const auto& [id, definition] : advancements_) {
        names.push_back(id);
    }
    return names;
}

const advancement* RealAdvancements::find(const Identifier& id) const {
    const auto found = advancements_.find(id);
    return found == advancements_.end() ? nullptr : &found->second;
}

std::vector<Identifier> RealAdvancements::descendantsOf(const Identifier& id) const {
    std::vector<Identifier> result{id};
    std::vector<Identifier> frontier{id};
    while (!frontier.empty()) {
        std::vector<Identifier> children;
        for (const auto& [name, definition] : advancements_) {
            if (definition.parent.has_value() && std::ranges::find(frontier, *definition.parent) != frontier.end()) {
                children.push_back(name);
            }
        }
        if (children.empty()) {
            break;
        }
        result.insert(result.end(), children.begin(), children.end());
        frontier = std::move(children);
    }
    return result;
}

std::vector<Identifier> RealAdvancements::ancestorsOf(const Identifier& id) const {
    std::vector<Identifier> result{id};
    const advancement* current = find(id);
    std::optional<Identifier> parent = current == nullptr ? std::nullopt : current->parent;
    while (parent.has_value()) {
        result.push_back(*parent);
        const advancement* above = find(*parent);
        parent = above == nullptr ? std::nullopt : above->parent;
    }
    return result;
}

void PlayerProgress::load(std::string_view uuid) {
    if (repository_ == nullptr) {
        return;
    }
    const storage::StoredProgress stored = repository_->load(uuid);
    std::map<Identifier, std::map<std::string, std::int64_t, std::less<>>> criteria;
    for (const auto& [id, names] : stored.criteria) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
        if (!parsed.has_value()) {
            continue;
        }
        const auto times = stored.obtained.find(id);
        for (const std::string& name : names) {
            std::int64_t when = 0;
            if (times != stored.obtained.end()) {
                if (const auto found = times->second.find(name); found != times->second.end()) {
                    when = found->second;
                }
            }
            criteria[*parsed].insert_or_assign(name, when);
        }
    }
    advancements_.insert_or_assign(std::string{uuid}, std::move(criteria));
    std::set<Identifier> recipes;
    for (const std::string& recipe : stored.recipes) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(recipe);
        if (parsed.has_value()) {
            recipes.insert(*parsed);
        }
    }
    recipes_.insert_or_assign(std::string{uuid}, std::move(recipes));
    std::set<Identifier> highlighted;
    for (const std::string& recipe : stored.highlighted) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(recipe);
        if (parsed.has_value()) {
            highlighted.insert(*parsed);
        }
    }
    highlighted_.insert_or_assign(std::string{uuid}, std::move(highlighted));
    bookSettings_.insert_or_assign(std::string{uuid}, stored.bookSettings);
}

void PlayerProgress::unload(std::string_view uuid) {
    if (const auto found = advancements_.find(uuid); found != advancements_.end()) {
        advancements_.erase(found);
    }
    if (const auto found = highlighted_.find(uuid); found != highlighted_.end()) {
        highlighted_.erase(found);
    }
    if (const auto found = bookSettings_.find(uuid); found != bookSettings_.end()) {
        bookSettings_.erase(found);
    }
    if (const auto found = recipes_.find(uuid); found != recipes_.end()) {
        recipes_.erase(found);
    }
}

bool PlayerProgress::grantCriterion(std::string_view uuid, const Identifier& advancementId,
                                    std::string_view criterion) {
    auto player = advancements_.find(uuid);
    if (player == advancements_.end()) {
        player =
            advancements_
                .emplace(std::string{uuid}, std::map<Identifier, std::map<std::string, std::int64_t, std::less<>>>{})
                .first;
    }
    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    const bool added = player->second[advancementId].emplace(criterion, now).second;
    if (added && repository_ != nullptr) {
        repository_->grantCriterion(uuid, advancementId.toString(), criterion, now);
    }
    if (added && changed_) {
        changed_(uuid, advancementId);
    }
    return added;
}

bool PlayerProgress::revokeCriterion(std::string_view uuid, const Identifier& advancementId,
                                     std::string_view criterion) {
    const auto player = advancements_.find(uuid);
    if (player == advancements_.end()) {
        return false;
    }
    const auto entry = player->second.find(advancementId);
    if (entry == player->second.end()) {
        return false;
    }
    const auto name = entry->second.find(criterion);
    if (name == entry->second.end()) {
        return false;
    }
    entry->second.erase(name);
    if (repository_ != nullptr) {
        repository_->revokeCriterion(uuid, advancementId.toString(), criterion);
    }
    if (changed_) {
        changed_(uuid, advancementId);
    }
    return true;
}

std::set<std::string, std::less<>> PlayerProgress::grantedCriteria(std::string_view uuid,
                                                                   const Identifier& advancementId) const {
    std::set<std::string, std::less<>> granted;
    for (const auto& [name, when] : obtained(uuid, advancementId)) {
        granted.insert(name);
    }
    return granted;
}

std::map<std::string, std::int64_t, std::less<>> PlayerProgress::obtained(std::string_view uuid,
                                                                          const Identifier& advancementId) const {
    const auto player = advancements_.find(uuid);
    if (player == advancements_.end()) {
        return {};
    }
    const auto entry = player->second.find(advancementId);
    return entry == player->second.end() ? std::map<std::string, std::int64_t, std::less<>>{} : entry->second;
}

std::vector<Identifier> PlayerProgress::started(std::string_view uuid) const {
    std::vector<Identifier> ids;
    if (const auto player = advancements_.find(uuid); player != advancements_.end()) {
        for (const auto& [id, criteria] : player->second) {
            if (!criteria.empty()) {
                ids.push_back(id);
            }
        }
    }
    return ids;
}

bool PlayerProgress::revokeAll(std::string_view uuid, const Identifier& advancementId) {
    const auto player = advancements_.find(uuid);
    if (player == advancements_.end()) {
        return false;
    }
    const auto entry = player->second.find(advancementId);
    if (entry == player->second.end()) {
        return false;
    }
    player->second.erase(entry);
    if (repository_ != nullptr) {
        repository_->revokeAdvancement(uuid, advancementId.toString());
    }
    if (changed_) {
        changed_(uuid, advancementId);
    }
    return true;
}

std::set<Identifier> PlayerProgress::knownRecipes(std::string_view uuid) const {
    const auto found = recipes_.find(uuid);
    return found == recipes_.end() ? std::set<Identifier>{} : found->second;
}

bool PlayerProgress::learn(std::string_view uuid, const Identifier& recipe) {
    auto player = recipes_.find(uuid);
    if (player == recipes_.end()) {
        player = recipes_.emplace(std::string{uuid}, std::set<Identifier>{}).first;
    }
    const bool added = player->second.insert(recipe).second;
    if (added) {
        highlighted_[std::string{uuid}].insert(recipe);
        if (repository_ != nullptr) {
            repository_->learnRecipe(uuid, recipe.toString());
        }
    }
    return added;
}

bool PlayerProgress::forget(std::string_view uuid, const Identifier& recipe) {
    const auto player = recipes_.find(uuid);
    if (player == recipes_.end()) {
        return false;
    }
    const auto found = player->second.find(recipe);
    if (found == player->second.end()) {
        return false;
    }
    player->second.erase(found);
    if (const auto marked = highlighted_.find(uuid); marked != highlighted_.end()) {
        marked->second.erase(recipe);
    }
    if (repository_ != nullptr) {
        repository_->forgetRecipe(uuid, recipe.toString());
    }
    return true;
}

std::set<Identifier> PlayerProgress::highlightedRecipes(std::string_view uuid) const {
    const auto found = highlighted_.find(uuid);
    return found == highlighted_.end() ? std::set<Identifier>{} : found->second;
}

void PlayerProgress::removeHighlight(std::string_view uuid, const Identifier& recipe) {
    const auto found = highlighted_.find(uuid);
    if (found == highlighted_.end() || found->second.erase(recipe) == 0) {
        return;
    }
    if (repository_ != nullptr) {
        repository_->removeHighlight(uuid, recipe.toString());
    }
}

std::map<std::int32_t, std::pair<bool, bool>> PlayerProgress::bookSettings(std::string_view uuid) const {
    const auto found = bookSettings_.find(uuid);
    return found == bookSettings_.end() ? std::map<std::int32_t, std::pair<bool, bool>>{} : found->second;
}

void PlayerProgress::setBookSetting(std::string_view uuid, std::int32_t book, bool open, bool filtering) {
    bookSettings_[std::string{uuid}][book] = {open, filtering};
    if (repository_ != nullptr) {
        repository_->setBookSetting(uuid, book, open, filtering);
    }
}

} // namespace basalt::server::runtime::world
