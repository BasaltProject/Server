#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace basalt::server::runtime::scoreboard {

using wire::protocol::play::DisplaySlot;
using wire::protocol::play::NumberFormat;
using wire::protocol::play::ObjectiveRenderType;
using wire::text::Component;

const std::vector<std::string>& Objective::criteria() {
    static const std::vector<std::string> registered = {"dummy",          "trigger", "deathCount", "playerKillCount",
                                                        "totalKillCount", "health",  "food",       "air",
                                                        "armor",          "xp",      "level"};
    return registered;
}

bool Objective::isReadOnly(std::string_view criterion) {
    return criterion == "health" || criterion == "food" || criterion == "air" || criterion == "armor" ||
           criterion == "xp" || criterion == "level";
}

ObjectiveRenderType Objective::defaultRenderType(std::string_view criterion) {
    return criterion == "health" ? ObjectiveRenderType::Hearts : ObjectiveRenderType::Integer;
}

std::vector<Objective*> Scoreboard::allObjectives() {
    std::vector<Objective*> every;
    every.reserve(objectives_.size());
    for (const std::unique_ptr<Objective>& objective : objectives_) {
        every.push_back(objective.get());
    }
    return every;
}

std::vector<const Objective*> Scoreboard::allObjectives() const {
    std::vector<const Objective*> every;
    every.reserve(objectives_.size());
    for (const std::unique_ptr<Objective>& objective : objectives_) {
        every.push_back(objective.get());
    }
    return every;
}

const Objective* Scoreboard::get(std::string_view name) const {
    for (const std::unique_ptr<Objective>& objective : objectives_) {
        if (objective->name == name) {
            return objective.get();
        }
    }
    return nullptr;
}

Objective* Scoreboard::get(std::string_view name) {
    for (const std::unique_ptr<Objective>& objective : objectives_) {
        if (objective->name == name) {
            return objective.get();
        }
    }
    return nullptr;
}

bool Scoreboard::add(Objective objective) {
    if (get(objective.name) != nullptr) {
        return false;
    }
    objectives_.push_back(std::make_unique<Objective>(std::move(objective)));
    return true;
}

std::unique_ptr<Objective> Scoreboard::remove(std::string_view name) {
    const auto at =
        std::ranges::find_if(objectives_, [name](const std::unique_ptr<Objective>& one) { return one->name == name; });
    if (at == objectives_.end()) {
        return nullptr;
    }
    std::unique_ptr<Objective> removed = std::move(*at);
    objectives_.erase(at);
    for (Holder& holder : holders_) {
        std::erase_if(holder.scores,
                      [name](const std::pair<std::string, Score>& score) { return score.first == name; });
    }
    std::erase_if(displaySlots_,
                  [name](const std::pair<DisplaySlot, std::string>& slot) { return slot.second == name; });
    return removed;
}

const std::string* Scoreboard::displayed(DisplaySlot slot) const {
    for (const std::pair<DisplaySlot, std::string>& shown : displaySlots_) {
        if (shown.first == slot) {
            return &shown.second;
        }
    }
    return nullptr;
}

void Scoreboard::display(DisplaySlot slot, std::optional<std::string> objectiveName) {
    std::erase_if(displaySlots_,
                  [slot](const std::pair<DisplaySlot, std::string>& shown) { return shown.first == slot; });
    if (objectiveName.has_value()) {
        displaySlots_.emplace_back(slot, std::move(*objectiveName));
    }
}

const Score* Scoreboard::find(std::string_view holder, std::string_view objectiveName) const {
    for (const Holder& one : holders_) {
        if (one.name != holder) {
            continue;
        }
        for (const std::pair<std::string, Score>& score : one.scores) {
            if (score.first == objectiveName) {
                return &score.second;
            }
        }
        return nullptr;
    }
    return nullptr;
}

Score* Scoreboard::find(std::string_view holder, std::string_view objectiveName) {
    return const_cast<Score*>(std::as_const(*this).find(holder, objectiveName));
}

Score& Scoreboard::create(std::string_view holder, std::string_view objectiveName) {
    if (Score* existing = find(holder, objectiveName); existing != nullptr) {
        return *existing;
    }
    const auto at = std::ranges::find_if(holders_, [holder](const Holder& one) { return one.name == holder; });
    Holder& owner =
        at != holders_.end() ? *at : holders_.emplace_back(Holder{.name = std::string{holder}, .scores = {}});
    owner.scores.emplace_back(std::string{objectiveName}, Score{});
    return owner.scores.back().second;
}

std::optional<std::int32_t> Scoreboard::score(std::string_view holder, std::string_view objectiveName) const {
    const Score* found = find(holder, objectiveName);
    return found != nullptr ? std::optional<std::int32_t>{found->value} : std::nullopt;
}

void Scoreboard::setScore(std::string_view holder, std::string_view objectiveName, std::int32_t value) {
    create(holder, objectiveName).value = value;
}

std::int32_t Scoreboard::addScore(std::string_view holder, std::string_view objectiveName, std::int32_t delta) {
    Score& entry = create(holder, objectiveName);
    const std::int64_t next = static_cast<std::int64_t>(entry.value) + delta;
    entry.value = static_cast<std::int32_t>(std::clamp<std::int64_t>(next, std::numeric_limits<std::int32_t>::min(),
                                                                     std::numeric_limits<std::int32_t>::max()));
    return entry.value;
}

void Scoreboard::resetScore(std::string_view holder, std::optional<std::string_view> objectiveName) {
    if (!objectiveName.has_value()) {
        std::erase_if(holders_, [holder](const Holder& one) { return one.name == holder; });
        return;
    }
    const auto at = std::ranges::find_if(holders_, [holder](const Holder& one) { return one.name == holder; });
    if (at == holders_.end()) {
        return;
    }
    const std::string_view wanted = *objectiveName;
    std::erase_if(at->scores, [wanted](const std::pair<std::string, Score>& score) { return score.first == wanted; });
    // parity: a holder with nothing left stops being tracked, rather than staying on as an empty
    // row that `/scoreboard players list` would still print.
    if (at->scores.empty()) {
        holders_.erase(at);
    }
}

std::vector<std::string> Scoreboard::holdersIn(std::string_view objectiveName) const {
    std::vector<std::string> named;
    for (const Holder& one : holders_) {
        const bool has = std::ranges::any_of(one.scores, [objectiveName](const std::pair<std::string, Score>& score) {
            return score.first == objectiveName;
        });
        if (has) {
            named.push_back(one.name);
        }
    }
    return named;
}

std::vector<std::string> Scoreboard::holders() const {
    std::vector<std::string> named;
    named.reserve(holders_.size());
    for (const Holder& one : holders_) {
        named.push_back(one.name);
    }
    return named;
}

std::vector<std::pair<std::string, std::int32_t>> Scoreboard::scoresOf(std::string_view holder) const {
    std::vector<std::pair<std::string, std::int32_t>> found;
    for (const Holder& one : holders_) {
        if (one.name != holder) {
            continue;
        }
        found.reserve(one.scores.size());
        for (const std::pair<std::string, Score>& score : one.scores) {
            found.emplace_back(score.first, score.second.value);
        }
        return found;
    }
    return found;
}

const Component* Scoreboard::displayNameOf(std::string_view holder, std::string_view objectiveName) const {
    const Score* found = find(holder, objectiveName);
    if (found == nullptr || !found->display.has_value()) {
        return nullptr;
    }
    return &*found->display;
}

void Scoreboard::setDisplayName(std::string_view holder, std::string_view objectiveName,
                                std::optional<Component> name) {
    create(holder, objectiveName).display = std::move(name);
}

const NumberFormat* Scoreboard::numberFormatOf(std::string_view holder, std::string_view objectiveName) const {
    const Score* found = find(holder, objectiveName);
    if (found == nullptr || !found->numberFormat.has_value()) {
        return nullptr;
    }
    return &*found->numberFormat;
}

void Scoreboard::setNumberFormat(std::string_view holder, std::string_view objectiveName,
                                 std::optional<NumberFormat> format) {
    create(holder, objectiveName).numberFormat = std::move(format);
}

void Scoreboard::enableTrigger(std::string_view holder, std::string_view objectiveName) {
    create(holder, objectiveName).locked = false;
}

bool Scoreboard::isTriggerEnabled(std::string_view holder, std::string_view objectiveName) const {
    const Score* found = find(holder, objectiveName);
    return found != nullptr && !found->locked;
}

void Scoreboard::disableTrigger(std::string_view holder, std::string_view objectiveName) {
    if (Score* found = find(holder, objectiveName); found != nullptr) {
        found->locked = true;
    }
}

} // namespace basalt::server::runtime::scoreboard
