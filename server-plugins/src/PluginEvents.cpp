#include <basalt/server/plugins/PluginEvents.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
namespace {
const PluginEventValue& nothing() {
    static const PluginEventValue absent{};
    return absent;
}
} // namespace

PluginEventType::PluginEventType(std::string name, bool cancellable, std::vector<std::string> fields,
                                 std::vector<std::string> mutableFields)
    : name_(std::move(name)), cancellable_(cancellable), fields_(std::move(fields)),
      mutable_(std::move(mutableFields)) {}

bool PluginEventType::isMutable(std::string_view field) const {
    return std::find(mutable_.begin(), mutable_.end(), field) != mutable_.end();
}

std::int32_t PluginEventType::indexOf(std::string_view field) const {
    const auto found = std::find(fields_.begin(), fields_.end(), field);
    return found == fields_.end() ? -1 : static_cast<std::int32_t>(found - fields_.begin());
}

PluginEvent::PluginEvent(const PluginEventType& type) : type_(&type), values_(type.fields().size()) {}

void PluginEvent::set(std::int32_t index, PluginEventValue value) {
    if (index >= 0 && static_cast<std::size_t>(index) < values_.size()) {
        values_[static_cast<std::size_t>(index)] = std::move(value);
    }
}

void PluginEvent::set(std::string_view field, PluginEventValue value) { set(type_->indexOf(field), std::move(value)); }

const PluginEventValue& PluginEvent::get(std::int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= values_.size()) {
        return nothing();
    }
    return values_[static_cast<std::size_t>(index)];
}

const PluginEventValue& PluginEvent::get(std::string_view field) const { return get(type_->indexOf(field)); }

void PluginEvent::cancel(std::optional<std::string> message) {
    cancelled_ = true;
    if (message.has_value()) {
        cancelMessage_ = std::move(message);
    }
}

void PluginEvent::restore() {
    cancelled_ = false;
    cancelMessage_.reset();
}

void PluginEvent::begin() {
    cancelled_ = false;
    cancelMessage_.reset();
    ++generation_;
    live_ = true;
}

void PluginEvent::end() { live_ = false; }

void PluginEvent::clear() {
    cancelled_ = false;
    cancelMessage_.reset();
}

void PluginHandle::remove() {
    if (remove_) {
        const Remove once = std::move(remove_);
        remove_ = nullptr;
        once();
    }
}

bool PluginEventBus::Handler::accepts(const PluginEvent& event) const {
    if (event.cancelled() && !wantsCancelled) {
        return false;
    }
    for (const auto& [field, expected] : filters) {
        if (event.get(field) != expected) {
            return false;
        }
    }
    return true;
}

void PluginEventBus::declare(PluginEventType type) {
    const std::string name = type.name();
    types_.insert_or_assign(name, std::move(type));
}

const PluginEventType* PluginEventBus::typeOf(std::string_view name) const {
    const auto found = types_.find(name);
    return found == types_.end() ? nullptr : &found->second;
}

std::vector<const PluginEventType*> PluginEventBus::declared() const {
    std::vector<const PluginEventType*> all;
    for (const auto& [name, type] : types_) {
        all.push_back(&type);
    }
    return all;
}

bool PluginEventBus::listening(const PluginEventType& type) const {
    const auto found = handlers_.find(type.name());
    return found != handlers_.end() && !found->second.empty();
}

PluginEvent& PluginEventBus::event(const PluginEventType& type) {
    const auto found = events_.find(type.name());
    if (found != events_.end()) {
        return *found->second;
    }
    return *events_.emplace(type.name(), std::make_unique<PluginEvent>(type)).first->second;
}

PluginHandle PluginEventBus::add(const PluginEventType& type, std::string pluginId, std::int32_t lane,
                                 PluginEventPriority priority, bool observer, bool wantsCancelled,
                                 std::map<std::string, PluginEventValue, std::less<>> filters, Call call) {
    const std::int64_t id = nextHandlerId_++;
    std::vector<Handler>& list = handlers_[type.name()];
    list.push_back(Handler{.id = id,
                           .pluginId = std::move(pluginId),
                           .lane = lane,
                           .priority = priority,
                           .observer = observer,
                           .wantsCancelled = wantsCancelled,
                           .filters = std::move(filters),
                           .call = std::move(call)});
    std::stable_sort(list.begin(), list.end(), [](const Handler& left, const Handler& right) {
        if (left.observer != right.observer) {
            return !left.observer;
        }
        return left.priority < right.priority;
    });

    const std::string name = type.name();
    return PluginHandle{[this, name, id] {
        const auto found = handlers_.find(name);
        if (found == handlers_.end()) {
            return;
        }
        std::erase_if(found->second, [id](const Handler& handler) { return handler.id == id; });
        if (found->second.empty()) {
            handlers_.erase(found);
        }
    }};
}

void PluginEventBus::removeAll(std::string_view pluginId) {
    for (auto entry = handlers_.begin(); entry != handlers_.end();) {
        std::erase_if(entry->second, [&](const Handler& handler) { return handler.pluginId == pluginId; });
        entry = entry->second.empty() ? handlers_.erase(entry) : std::next(entry);
    }
}

bool PluginEventBus::fire(PluginEvent& event) {
    const auto found = handlers_.find(event.type().name());
    if (found == handlers_.end() || found->second.empty()) {
        event.clear();
        return true;
    }
    event.begin();
    std::size_t index = 0;
    while (true) {
        const auto list = handlers_.find(event.type().name());
        if (list == handlers_.end() || index >= list->second.size()) {
            break;
        }
        // Copied, because the call may register or unregister handlers and move the vector under us.
        const Call call = list->second[index].call;
        const bool accepted = list->second[index].accepts(event);
        ++index;
        if (accepted && call) {
            call(event);
        }
    }
    event.end();
    return !event.cancelled();
}
} // namespace basalt::server::plugins
