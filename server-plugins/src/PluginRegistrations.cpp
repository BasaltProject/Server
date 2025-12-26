#include <basalt/server/plugins/PluginRegistrations.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::plugins {

std::vector<std::string> PluginRegistrations::summary() const {
    std::vector<std::string> names;
    names.reserve(entries_->size());
    for (const Entry& entry : *entries_) {
        names.push_back(entry.what);
    }
    return names;
}

PluginHandle PluginRegistrations::add(std::string what, std::function<void()> undo) {
    const std::uint64_t id = nextId_++;
    entries_->push_back(Entry{.what = std::move(what), .undo = std::move(undo)});
    ids_->push_back(id);
    return PluginHandle{[entries = entries_, ids = ids_, id] {
        const auto found = std::ranges::find(*ids, id);
        if (found == ids->end()) {
            return;
        }
        const auto index = static_cast<std::size_t>(std::distance(ids->begin(), found));
        const std::function<void()> undo = (*entries)[index].undo;
        entries->erase(entries->begin() + static_cast<std::ptrdiff_t>(index));
        ids->erase(found);
        if (undo) {
            undo();
        }
    }};
}

void PluginRegistrations::retireAll(const OnFailure& onFailure) {
    for (auto entry = entries_->rbegin(); entry != entries_->rend(); ++entry) {
        if (!entry->undo) {
            continue;
        }
        try {
            entry->undo();
        } catch (const std::exception& error) {
            if (onFailure) {
                onFailure(entry->what, error.what());
            }
        }
    }
    entries_->clear();
    ids_->clear();
}

} // namespace basalt::server::plugins
