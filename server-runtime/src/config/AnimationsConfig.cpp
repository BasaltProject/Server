#include <basalt/server/runtime/config/AnimationsConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::config {

using wire::common::placeholder::lowercaseAscii;
using wire::common::toml::TomlTable;

const std::string& Animation::frameAt(std::int64_t epochMillis) const {
    // A still picture, which the arithmetic below would also answer: this only says so plainly.
    if (frames.size() == 1) {
        return frames.front();
    }
    // The clock decides, not a counter: everyone looking at it sees the same frame, and a player
    // who reconnects does not restart it.
    const std::int64_t step = (epochMillis / changeIntervalMillis) % static_cast<std::int64_t>(frames.size());
    return frames[static_cast<std::size_t>(step)];
}

std::vector<std::string> AnimationsConfig::names() const {
    std::vector<std::string> named;
    named.reserve(animations_.size());
    for (const Animation& animation : animations_) {
        named.push_back(animation.name);
    }
    return named;
}

const Animation* AnimationsConfig::get(std::string_view name) const {
    const std::string lowercase = lowercaseAscii(name);
    for (const Animation& animation : animations_) {
        if (animation.name == lowercase) {
            return &animation;
        }
    }
    return nullptr;
}

std::expected<AnimationsConfig, std::string> AnimationsConfig::load(const ConfigDirectory& directory,
                                                                    const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, kAnimationsDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }

    std::vector<Animation> animations;
    if (const TomlTable* declared = root->table("animations"); declared != nullptr) {
        for (const std::string& name : declared->keys()) {
            const TomlTable* table = declared->table(name);
            if (table == nullptr) {
                continue;
            }
            std::vector<std::string> frames = table->stringList("frames");
            if (frames.empty()) {
                logger.warn(
                    [&name, &path] { return "Animation '" + name + "' in " + path + " has no frames; skipping it."; });
                continue;
            }
            const std::int64_t interval =
                std::max<std::int64_t>(table->integer("change-interval-ms").value_or(200), kMinimumIntervalMillis);
            animations.push_back(
                Animation{.name = lowercaseAscii(name), .frames = std::move(frames), .changeIntervalMillis = interval});
        }
    }

    if (!animations.empty()) {
        logger.debug([&animations, &path] {
            return "Loaded " + std::to_string(animations.size()) + " animation(s) from " + path + ".";
        });
    }
    return AnimationsConfig{std::move(animations)};
}

} // namespace basalt::server::runtime::config
