#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::server::plugins {

/// When a handler runs relative to the others.
///
/// Five, not six: Bukkit's `MONITOR` is not a priority, it is "I want to look without touching", and
/// that is what an observer is instead.
enum class PluginEventPriority : std::uint8_t { First, Early, Normal, Late, Last };

[[nodiscard]] std::string_view priorityName(PluginEventPriority priority);

[[nodiscard]] std::optional<PluginEventPriority> priorityOf(std::string_view name);

/// What an event field can hold. Absent is nil, which is what a field the server did not fill reads
/// as, and what a filter on it compares against.
using PluginEventValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

/// One kind of event: what it is called, what it carries, and whether it can be stopped.
///
/// The fields are declared rather than discovered so that the documentation, the editor's completion
/// and the audit against Bukkit all come from one place, and so that a plugin reading a field that
/// does not exist finds out at once instead of getting a silent nil.
class PluginEventType {
public:
    PluginEventType(std::string name, bool cancellable, std::vector<std::string> fields,
                    std::vector<std::string> mutableFields = {});

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] bool cancellable() const noexcept { return cancellable_; }

    /// In the order the server fills them, which is the order they are read back.
    [[nodiscard]] const std::vector<std::string>& fields() const noexcept { return fields_; }

    /// The ones a handler may write, which the server reads back after dispatch.
    [[nodiscard]] bool isMutable(std::string_view field) const;

    /// Where a field sits, or -1 when this event does not have one by that name.
    [[nodiscard]] std::int32_t indexOf(std::string_view field) const;

private:
    std::string name_;
    bool cancellable_;
    std::vector<std::string> fields_;
    std::vector<std::string> mutable_;
};

/// One event being fired.
///
/// There is one of these per type and lane, reused: firing an event does not make one, so an event
/// nobody listens to costs a comparison and an event with listeners costs what its values cost. The
/// other half of that bargain is that it stops being valid the moment the handler returns, which is
/// checked rather than left to chance: keeping one and reading it a tick later is a mistake with a
/// message, not a value that quietly means nothing.
class PluginEvent {
public:
    explicit PluginEvent(const PluginEventType& type);

    [[nodiscard]] const PluginEventType& type() const noexcept { return *type_; }

    void set(std::int32_t index, PluginEventValue value);
    void set(std::string_view field, PluginEventValue value);

    [[nodiscard]] const PluginEventValue& get(std::int32_t index) const;
    [[nodiscard]] const PluginEventValue& get(std::string_view field) const;

    [[nodiscard]] bool cancelled() const noexcept { return cancelled_; }

    /// What to tell whoever caused it, when the handler bothered to say.
    [[nodiscard]] const std::optional<std::string>& cancelMessage() const noexcept { return cancelMessage_; }

    void cancel(std::optional<std::string> message = std::nullopt);

    /// Bumped on every fire, so a handler that kept the event can be told it is stale.
    [[nodiscard]] std::int64_t generation() const noexcept { return generation_; }

    [[nodiscard]] bool live() const noexcept { return live_; }

private:
    friend class PluginEventBus;

    /// Readies it for another fire. Values are overwritten by the caller straight after.
    void begin();
    void end();
    /// What the fast path does: no handlers, nothing to answer, no stale answer left behind.
    void clear();

    const PluginEventType* type_;
    std::vector<PluginEventValue> values_;
    std::int64_t generation_ = 0;
    bool cancelled_ = false;
    std::optional<std::string> cancelMessage_;
    bool live_ = false;
};

/// What a plugin gets back when it registers, and what it calls to stop listening.
///
/// Named `remove` rather than the frozen Kotlin's `cancel`: on this class the word already means
/// stopping an event, and one word meaning two things in the same file is how a handler gets
/// unregistered when somebody meant to veto a block break.
class PluginHandle {
public:
    using Remove = std::function<void()>;

    PluginHandle() = default;

    explicit PluginHandle(Remove remove) : remove_(std::move(remove)) {}

    void remove();

private:
    Remove remove_;
};

/// Who is listening to what.
///
/// Two kinds of handler, and the difference is not a convention: a decider runs where the server
/// asked the question and answers it, so it may cancel and may not pause; an observer runs after the
/// fact, so it may take as long as it likes and may not change anything. That is the whole of the
/// split, and it is what lets a protection plugin answer inside the tick while a statistics one goes
/// to disk without holding anyone up.
class PluginEventBus {
public:
    using Call = std::function<void(PluginEvent&)>;

    /// What may be listened to. A name outside this is an error at registration, not a silent no-op.
    void declare(PluginEventType type);

    [[nodiscard]] const PluginEventType* typeOf(std::string_view name) const;

    [[nodiscard]] std::vector<const PluginEventType*> declared() const;

    /// The one question the hot path asks.
    [[nodiscard]] bool listening(const PluginEventType& type) const;

    /// The reusable event of this type, to fill in and fire.
    ///
    /// One per type rather than one per fire: that is what makes firing free of allocation. It is
    /// handed out rather than made by the caller so that nothing else has to know it is shared.
    [[nodiscard]] PluginEvent& event(const PluginEventType& type);

    /// Registers a handler. `filters` are field equalities that have to hold, so a handler is not
    /// woken to run one `if`.
    [[nodiscard]] PluginHandle add(const PluginEventType& type, std::string pluginId, std::int32_t lane,
                                   PluginEventPriority priority, bool observer, bool wantsCancelled,
                                   std::map<std::string, PluginEventValue, std::less<>> filters, Call call);

    /// Drops everything one plugin registered, for an unload that has to leave nothing behind.
    void removeAll(std::string_view pluginId);

    /// Runs the handlers of `event` and answers whether it may go ahead.
    bool fire(PluginEvent& event);

private:
    struct Handler {
        std::int64_t id = 0;
        std::string pluginId;
        std::int32_t lane = 0;
        PluginEventPriority priority = PluginEventPriority::Normal;
        bool observer = false;
        /// Whether it wants events another handler already cancelled. Off by default, unlike Bukkit.
        bool wantsCancelled = false;
        std::map<std::string, PluginEventValue, std::less<>> filters;
        Call call;

        [[nodiscard]] bool accepts(const PluginEvent& event) const;
    };

    std::map<std::string, std::vector<Handler>, std::less<>> handlers_;
    std::map<std::string, PluginEventType, std::less<>> types_;
    std::map<std::string, std::unique_ptr<PluginEvent>, std::less<>> events_;
    std::int64_t nextHandlerId_ = 0;
};

/// How the server fires an event, and what it costs when nobody is listening: with no bus or no
/// handler the whole thing is a null check and a boolean, and nothing is allocated. That is the
/// promise the event catalogue makes to the tick loop, kept in one place.
///
/// Answers the event when it fired, so the caller can read `cancelled` or a field a handler changed,
/// and null when there was nobody to tell.
template <typename Fill>
[[nodiscard]] PluginEvent* emit(PluginEventBus* bus, const PluginEventType* type, Fill&& fill) {
    if (bus == nullptr || type == nullptr || !bus->listening(*type)) {
        return nullptr;
    }
    PluginEvent& event = bus->event(*type);
    fill(event);
    static_cast<void>(bus->fire(event));
    return &event;
}

} // namespace basalt::server::plugins
