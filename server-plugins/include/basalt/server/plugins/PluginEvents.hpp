#pragma once

#include <basalt/wire/plugins/EventPriority.hpp>

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
using PluginEventPriority = wire::plugins::EventPriority;
using wire::plugins::priorityName;
using wire::plugins::priorityOf;

using PluginEventValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

class PluginEventType {
public:
    PluginEventType(std::string name, bool cancellable, std::vector<std::string> fields,
                    std::vector<std::string> mutableFields = {});

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] bool cancellable() const noexcept { return cancellable_; }

    [[nodiscard]] const std::vector<std::string>& fields() const noexcept { return fields_; }

    [[nodiscard]] bool isMutable(std::string_view field) const;

    [[nodiscard]] std::int32_t indexOf(std::string_view field) const;

private:
    std::string name_;
    bool cancellable_;
    std::vector<std::string> fields_;
    std::vector<std::string> mutable_;
};

class PluginEvent {
public:
    explicit PluginEvent(const PluginEventType& type);

    [[nodiscard]] const PluginEventType& type() const noexcept { return *type_; }

    void set(std::int32_t index, PluginEventValue value);
    void set(std::string_view field, PluginEventValue value);

    [[nodiscard]] const PluginEventValue& get(std::int32_t index) const;
    [[nodiscard]] const PluginEventValue& get(std::string_view field) const;

    [[nodiscard]] bool cancelled() const noexcept { return cancelled_; }

    [[nodiscard]] const std::optional<std::string>& cancelMessage() const noexcept { return cancelMessage_; }

    void cancel(std::optional<std::string> message = std::nullopt);

    /// Takes back a cancellation, as a listener that runs later and does not want the event stopped.
    void restore();

    [[nodiscard]] std::int64_t generation() const noexcept { return generation_; }

    [[nodiscard]] bool live() const noexcept { return live_; }

private:
    friend class PluginEventBus;

    void begin();
    void end();
    void clear();

    const PluginEventType* type_;
    std::vector<PluginEventValue> values_;
    std::int64_t generation_ = 0;
    bool cancelled_ = false;
    std::optional<std::string> cancelMessage_;
    bool live_ = false;
};

class PluginHandle {
public:
    using Remove = std::function<void()>;

    PluginHandle() = default;

    explicit PluginHandle(Remove remove) : remove_(std::move(remove)) {}

    void remove();

private:
    Remove remove_;
};

class PluginEventBus {
public:
    using Call = std::function<void(PluginEvent&)>;

    void declare(PluginEventType type);

    [[nodiscard]] const PluginEventType* typeOf(std::string_view name) const;

    [[nodiscard]] std::vector<const PluginEventType*> declared() const;

    [[nodiscard]] bool listening(const PluginEventType& type) const;

    [[nodiscard]] PluginEvent& event(const PluginEventType& type);

    [[nodiscard]] PluginHandle add(const PluginEventType& type, std::string pluginId, std::int32_t lane,
                                   PluginEventPriority priority, bool observer, bool wantsCancelled,
                                   std::map<std::string, PluginEventValue, std::less<>> filters, Call call);

    void removeAll(std::string_view pluginId);

    bool fire(PluginEvent& event);

private:
    struct Handler {
        std::int64_t id = 0;
        std::string pluginId;
        std::int32_t lane = 0;
        PluginEventPriority priority = PluginEventPriority::Normal;
        bool observer = false;
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
