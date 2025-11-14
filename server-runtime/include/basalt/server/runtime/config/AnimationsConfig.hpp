#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::config {

/// One animation: its frames, and how long each is shown.
///
/// There is no state here on purpose. The frame is a function of the clock, which is what makes an
/// animation the same for everyone looking at it, costs nothing per player, and survives a player
/// reconnecting without restarting.
struct Animation {
    std::string name;
    std::vector<std::string> frames;
    std::int64_t changeIntervalMillis = 200;

    [[nodiscard]] const std::string& frameAt(std::int64_t epochMillis) const;
};

/// `config/animations.toml`.
class AnimationsConfig {
public:
    static constexpr std::string_view kFileName = "animations.toml";

    /// A floor low enough not to silently rewrite what the file asks for.
    ///
    /// It used to be 50 (one tick, the fastest anything can redraw), which quietly turned a
    /// `change-interval-ms = 20` into 50: the file said one thing and the server did another.
    /// Asking for less than a tick is a legitimate choice, it just skips frames.
    static constexpr std::int64_t kMinimumIntervalMillis = 1;

    explicit AnimationsConfig(std::vector<Animation> animations = {}) : animations_(std::move(animations)) {}

    [[nodiscard]] static std::expected<AnimationsConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] std::vector<std::string> names() const;

    [[nodiscard]] const Animation* get(std::string_view name) const;

private:
    std::vector<Animation> animations_;
};

} // namespace basalt::server::runtime::config
