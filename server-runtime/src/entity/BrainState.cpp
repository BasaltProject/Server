#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>
#include <basalt/server/runtime/entity/brain/BrainState.hpp>

#include <algorithm>

namespace basalt::server::runtime::entity::brain {
namespace {

constexpr std::int64_t kScheduleUpdateDelay = 20;

[[nodiscard]] std::string_view activityName(ai::villagerSchedule::Activity activity) {
    using ai::villagerSchedule::Activity;
    switch (activity) {
    case Activity::Work:
        return "work";
    case Activity::Meet:
        return "meet";
    case Activity::Rest:
        return "rest";
    case Activity::Play:
        return "play";
    case Activity::Idle:
        break;
    }
    return "idle";
}

} // namespace

BrainState::BrainState() : core_{"core"} { useDefaultActivity(); }

void BrainState::registerMemory(std::string_view key) { memories_.try_emplace(std::string{key}); }

bool BrainState::isRegistered(std::string_view key) const { return memories_.contains(key); }

bool BrainState::set(std::string_view key, Value value, std::int64_t ttl) {
    const auto found = memories_.find(key);
    if (found == memories_.end()) {
        return false;
    }
    if (value.kind == Value::Kind::List && value.items.empty()) {
        found->second = Slot{};
        return true;
    }
    found->second = Slot{.value = std::move(value), .ttl = ttl};
    return true;
}

const Value* BrainState::get(std::string_view key) const {
    const auto found = memories_.find(key);
    return found == memories_.end() || !found->second.value.has_value() ? nullptr : &*found->second.value;
}

void BrainState::erase(std::string_view key) {
    if (const auto found = memories_.find(key); found != memories_.end()) {
        found->second = Slot{};
    }
}

void BrainState::clearMemories() {
    for (auto& [key, slot] : memories_) {
        slot = Slot{};
    }
}

bool BrainState::has(std::string_view key) const { return get(key) != nullptr; }

std::optional<std::int64_t> BrainState::timeToLive(std::string_view key) const {
    const auto found = memories_.find(key);
    if (found == memories_.end() || !found->second.value.has_value()) {
        return std::nullopt;
    }
    return found->second.ttl;
}

bool BrainState::check(std::string_view key, MemoryStatus status) const {
    const auto found = memories_.find(key);
    if (found == memories_.end()) {
        return false;
    }
    const bool present = found->second.value.has_value();
    return status == MemoryStatus::Registered || (status == MemoryStatus::Present && present) ||
           (status == MemoryStatus::Absent && !present);
}

std::vector<std::string> BrainState::keys() const {
    std::vector<std::string> out;
    out.reserve(memories_.size());
    for (const auto& [key, slot] : memories_) {
        out.push_back(key);
    }
    return out;
}

void BrainState::tickMemories() {
    for (auto& [key, slot] : memories_) {
        if (!slot.value.has_value() || slot.ttl == kForever) {
            continue;
        }
        if (slot.ttl <= 0) {
            slot = Slot{};
        } else {
            slot.ttl--;
        }
    }
}

void BrainState::tick(std::int64_t gameTime, std::int64_t dayTime, bool baby) {
    if (gameTime == lastTick_) {
        return;
    }
    lastTick_ = gameTime;
    tickMemories();
    if (!nativeOwned_) {
        updateFromSchedule(gameTime, dayTime, baby);
    }
}

void BrainState::setRunning(std::string_view key, bool running) {
    if (running) {
        running_.emplace(key);
    } else if (const auto found = running_.find(key); found != running_.end()) {
        running_.erase(found);
    }
}

void BrainState::defineActivity(std::string_view name, std::vector<Requirement> requirements,
                                std::vector<std::string> eraseWhenStopped) {
    for (const Requirement& requirement : requirements) {
        registerMemory(requirement.memory);
    }
    activities_[std::string{name}] = Activity{std::move(requirements), std::move(eraseWhenStopped)};
}

void BrainState::addBehavior(std::string_view activity, std::int32_t priority, std::string_view key) {
    behaviors_.emplace(priority, std::pair{std::string{activity}, std::string{key}});
}

void BrainState::removeBehaviors() {
    behaviors_.clear();
    running_.clear();
}

std::vector<BehaviorRow> BrainState::behaviors() const {
    std::vector<BehaviorRow> rows;
    for (const auto& [priority, entry] : behaviors_) {
        rows.push_back(BehaviorRow{.activity = entry.first, .priority = priority, .key = entry.second});
    }
    return rows;
}

void BrainState::setCoreActivities(StringSet core) { core_ = std::move(core); }

void BrainState::setDefaultActivity(std::string_view name) { default_ = name; }

void BrainState::useDefaultActivity() { setActive(default_); }

void BrainState::setActive(std::string_view name) {
    if (isActive(name)) {
        return;
    }
    for (const std::string& old : active_) {
        if (old == name) {
            continue;
        }
        if (const auto found = activities_.find(old); found != activities_.end()) {
            for (const std::string& key : found->second.eraseWhenStopped) {
                erase(key);
            }
        }
    }
    active_ = core_;
    active_.insert(std::string{name});
}

bool BrainState::requirementsMet(std::string_view activity) const {
    const auto found = activities_.find(activity);
    if (found == activities_.end()) {
        return false;
    }
    return std::ranges::all_of(found->second.requirements,
                               [this](const Requirement& one) { return check(one.memory, one.status); });
}

void BrainState::setActiveIfPossible(std::string_view name) {
    if (requirementsMet(name)) {
        setActive(name);
    } else {
        useDefaultActivity();
    }
}

void BrainState::setActiveToFirstValid(const std::vector<std::string>& names) {
    for (const std::string& name : names) {
        if (requirementsMet(name)) {
            setActive(name);
            return;
        }
    }
}

bool BrainState::isActive(std::string_view name) const { return active_.contains(name); }

std::optional<std::string> BrainState::activeNonCore() const {
    for (const std::string& name : active_) {
        if (!core_.contains(name)) {
            return name;
        }
    }
    return std::nullopt;
}

bool BrainState::hasActivity(std::string_view name) const { return activities_.contains(name); }

void BrainState::updateFromSchedule(std::int64_t gameTime, std::int64_t dayTime, bool baby) {
    if (gameTime - lastScheduleUpdate_ <= kScheduleUpdateDelay) {
        return;
    }
    lastScheduleUpdate_ = gameTime;
    std::string scheduled = "idle";
    if (schedule_ == "villager") {
        scheduled = activityName(ai::villagerSchedule::activityAt(dayTime, baby));
    }
    if (!isActive(scheduled)) {
        setActiveIfPossible(scheduled);
    }
}

} // namespace basalt::server::runtime::entity::brain
