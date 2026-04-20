#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::config {
class AnimationsConfig;
}

namespace basalt::server::runtime::placeholder {

struct AnimationPlaceholderSources {
    std::function<const config::AnimationsConfig&()> animations;
    std::function<const wire::common::placeholder::PlaceholderRegistry&()> registry;
    std::function<std::int64_t()> nowMillis;
};

constexpr std::string_view kAnimationIdentifier = "basalt_animation";

[[nodiscard]] wire::common::placeholder::PlaceholderProvider animationPlaceholders(AnimationPlaceholderSources sources);

[[nodiscard]] wire::common::placeholder::PlaceholderCatalog
animationPlaceholderNames(std::function<const config::AnimationsConfig&()> animations);

} // namespace basalt::server::runtime::placeholder
