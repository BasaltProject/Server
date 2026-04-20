#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
class PluginRegistrations {
public:
    using OnFailure = std::function<void(std::string_view what, std::string_view error)>;

    [[nodiscard]] std::size_t size() const noexcept { return entries_->size(); }

    [[nodiscard]] std::vector<std::string> summary() const;

    PluginHandle add(std::string what, std::function<void()> undo);

    void retireAll(const OnFailure& onFailure);

private:
    struct Entry {
        std::string what;
        std::function<void()> undo;
    };

    std::shared_ptr<std::vector<Entry>> entries_ = std::make_shared<std::vector<Entry>>();
    std::shared_ptr<std::vector<std::uint64_t>> ids_ = std::make_shared<std::vector<std::uint64_t>>();
    std::uint64_t nextId_ = 0;
};
} // namespace basalt::server::plugins
