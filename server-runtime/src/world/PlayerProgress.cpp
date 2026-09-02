#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/world/advancement/AdvancementJson.hpp>

#include <algorithm>
#include <expected>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using advancement = server::world::advancement::Advancement;

constexpr std::string_view kAdvancementPrefix = "data/minecraft/advancement/";
constexpr std::string_view kJsonSuffix = ".json";

} // namespace

RealAdvancements::RealAdvancements(std::map<Identifier, advancement> definitions)
    : advancements_(std::move(definitions)) {}

RealAdvancements::RealAdvancements(const Jar& jar, wire::common::Logger logger) {
    if (!jar.entriesUnder || !jar.readText) {
        return;
    }
    for (const std::string& entry : jar.entriesUnder(kAdvancementPrefix)) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        const std::optional<std::string> text = jar.readText(entry);
        if (!text.has_value()) {
            logger.debug([&] { return "Skipped advancement " + entry + ": unreadable"; });
            continue;
        }
        const std::expected<advancement, server::world::advancement::AdvancementParseError> parsed =
            server::world::advancement::advancementJson::parse(*text);
        if (!parsed.has_value()) {
            logger.debug([&] { return "Skipped advancement " + entry + ": malformed"; });
            continue;
        }
        std::string_view path = entry;
        path.remove_prefix(kAdvancementPrefix.size());
        path.remove_suffix(kJsonSuffix.size());
        advancements_.insert_or_assign(Identifier::vanilla(std::string{path}), *parsed);
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
    std::map<Identifier, std::set<std::string, std::less<>>> criteria;
    for (const auto& [id, names] : stored.criteria) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
        if (parsed.has_value()) {
            criteria.insert_or_assign(*parsed, names);
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
}

void PlayerProgress::unload(std::string_view uuid) {
    if (const auto found = advancements_.find(uuid); found != advancements_.end()) {
        advancements_.erase(found);
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
            advancements_.emplace(std::string{uuid}, std::map<Identifier, std::set<std::string, std::less<>>>{}).first;
    }
    const bool added = player->second[advancementId].emplace(criterion).second;
    if (added && repository_ != nullptr) {
        repository_->grantCriterion(uuid, advancementId.toString(), criterion);
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
    return true;
}

std::set<std::string, std::less<>> PlayerProgress::grantedCriteria(std::string_view uuid,
                                                                   const Identifier& advancementId) const {
    const auto player = advancements_.find(uuid);
    if (player == advancements_.end()) {
        return {};
    }
    const auto entry = player->second.find(advancementId);
    return entry == player->second.end() ? std::set<std::string, std::less<>>{} : entry->second;
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
    if (added && repository_ != nullptr) {
        repository_->learnRecipe(uuid, recipe.toString());
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
    if (repository_ != nullptr) {
        repository_->forgetRecipe(uuid, recipe.toString());
    }
    return true;
}

} // namespace basalt::server::runtime::world
