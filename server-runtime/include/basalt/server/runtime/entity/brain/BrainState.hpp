#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity::brain {

inline constexpr std::int64_t kForever = -1;

/// A memory value: scalars, an entity or a place, a walk target, or a list of those.
struct Value {
    enum class Kind : std::uint8_t { Marker, Bool, Int, Real, Text, Entity, Position, Walk, List };

    Kind kind = Kind::Marker;
    bool flag = false;
    std::int64_t whole = 0;
    double real = 0.0;
    std::string text;
    std::array<double, 3> at{};
    std::int32_t closeEnough = 0;
    std::vector<Value> items;

    bool operator==(const Value&) const = default;
};

using StringSet = std::set<std::string, std::less<>>;

enum class MemoryStatus : std::uint8_t { Registered, Present, Absent };

struct Requirement {
    std::string memory;
    MemoryStatus status = MemoryStatus::Present;
};

struct BehaviorRow {
    std::string activity;
    std::int32_t priority = 0;
    std::string key;
};

struct SensorSpec {
    std::string name;
    double range = 16.0;
    std::int32_t interval = 20;
    std::int64_t nextScan = 0;
    bool started = false;
};

/// parity: `Brain`: memories with expiry, activities with requirements, and the schedule that picks one.
class BrainState {
public:
    BrainState();

    void registerMemory(std::string_view key);
    [[nodiscard]] bool isRegistered(std::string_view key) const;
    /// An unregistered key is ignored and an empty list erases, like `Brain.setMemoryInternal`.
    bool set(std::string_view key, Value value, std::int64_t ttl = kForever);
    [[nodiscard]] const Value* get(std::string_view key) const;
    void erase(std::string_view key);
    void clearMemories();
    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] std::optional<std::int64_t> timeToLive(std::string_view key) const;
    [[nodiscard]] bool check(std::string_view key, MemoryStatus status) const;
    [[nodiscard]] std::vector<std::string> keys() const;

    /// parity: `MemorySlot.tick` and `updateActivityFromSchedule`; runs once per game tick however often it is called.
    void tick(std::int64_t gameTime, std::int64_t dayTime, bool baby);

    void defineActivity(std::string_view name, std::vector<Requirement> requirements,
                        std::vector<std::string> eraseWhenStopped);
    void addBehavior(std::string_view activity, std::int32_t priority, std::string_view key);
    void removeBehaviors();
    [[nodiscard]] std::vector<BehaviorRow> behaviors() const;

    [[nodiscard]] bool hasBehaviors() const { return !behaviors_.empty(); }

    void setCoreActivities(StringSet core);
    void setDefaultActivity(std::string_view name);
    void useDefaultActivity();
    void setActive(std::string_view name);
    void setActiveIfPossible(std::string_view name);
    void setActiveToFirstValid(const std::vector<std::string>& names);
    [[nodiscard]] bool isActive(std::string_view name) const;

    [[nodiscard]] const StringSet& active() const { return active_; }

    [[nodiscard]] std::optional<std::string> activeNonCore() const;
    [[nodiscard]] bool hasActivity(std::string_view name) const;

    void setSchedule(std::string_view name) { schedule_ = name; }

    /// A native owner publishes the activity and memories itself, so the schedule never overrides them.
    void setNativeOwned(bool owned) { nativeOwned_ = owned; }

    [[nodiscard]] bool nativeOwned() const { return nativeOwned_; }

    void requestDrive() { driveRequested_ = true; }

    [[nodiscard]] bool takeDrive() { return std::exchange(driveRequested_, false); }

    /// True once per game tick however often it is asked, so a plugin driving a brain never doubles the native run.
    [[nodiscard]] bool claimRun(std::int64_t gameTime) { return std::exchange(lastRun_, gameTime) != gameTime; }

    void replaceActive(StringSet names) { active_ = std::move(names); }

    [[nodiscard]] bool isRunning(std::string_view key) const { return running_.contains(key); }

    void setRunning(std::string_view key, bool running);

    [[nodiscard]] const StringSet& runningBehaviors() const { return running_; }

    [[nodiscard]] const std::string& schedule() const { return schedule_; }

    /// parity: `Brain.updateActivityFromSchedule`, at most every twenty ticks.
    void updateFromSchedule(std::int64_t gameTime, std::int64_t dayTime, bool baby);

    [[nodiscard]] std::vector<SensorSpec>& sensors() { return sensors_; }

    [[nodiscard]] const std::vector<SensorSpec>& sensors() const { return sensors_; }

private:
    struct Slot {
        std::optional<Value> value;
        std::int64_t ttl = kForever;
    };

    struct Activity {
        std::vector<Requirement> requirements;
        std::vector<std::string> eraseWhenStopped;
    };

    void tickMemories();
    [[nodiscard]] bool requirementsMet(std::string_view activity) const;

    std::map<std::string, Slot, std::less<>> memories_;
    std::map<std::string, Activity, std::less<>> activities_;
    std::multimap<std::int32_t, std::pair<std::string, std::string>> behaviors_;
    StringSet core_;
    StringSet active_;
    std::string default_ = "idle";
    std::string schedule_;
    std::int64_t lastScheduleUpdate_ = -9999;
    std::int64_t lastTick_ = -1;
    std::vector<SensorSpec> sensors_;
    StringSet running_;
    bool nativeOwned_ = false;
    bool driveRequested_ = false;
    std::int64_t lastRun_ = -1;
};

} // namespace basalt::server::runtime::entity::brain
