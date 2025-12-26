#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

/// Everything one plugin has registered, and how to take it back out.
///
/// This is the whole of why unloading works: nothing a plugin registers is allowed to exist outside
/// this list, so putting the server back the way it was is walking it backwards. The rule that keeps
/// it true is worth stating plainly: **what cannot be retired cannot be registered.**
class PluginRegistrations {
public:
    /// What a retirement that threw is reported through.
    using OnFailure = std::function<void(std::string_view what, std::string_view error)>;

    [[nodiscard]] std::size_t size() const noexcept { return entries_->size(); }

    /// What is registered, for `/plugins` and for a test that wants to see nothing is left.
    [[nodiscard]] std::vector<std::string> summary() const;

    PluginHandle add(std::string what, std::function<void()> undo);

    /// Retires everything, newest first, and keeps going when one of them throws.
    ///
    /// Newest first because a later registration may lean on an earlier one; and it keeps going
    /// because a plugin left half-unloaded is worse than one whose cleanup complained.
    void retireAll(const OnFailure& onFailure);

private:
    struct Entry {
        std::string what;
        std::function<void()> undo;
    };

    /// Shared so a handle outliving this object retires nothing rather than reaching freed memory.
    std::shared_ptr<std::vector<Entry>> entries_ = std::make_shared<std::vector<Entry>>();
    /// Which entry a handle refers to, since the vector shifts as entries are retired.
    std::shared_ptr<std::vector<std::uint64_t>> ids_ = std::make_shared<std::vector<std::uint64_t>>();
    std::uint64_t nextId_ = 0;
};

} // namespace basalt::server::plugins
