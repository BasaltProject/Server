#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <functional>

namespace basalt::server::runtime::config {
class AnimationsConfig;
}

namespace basalt::server::runtime::placeholder {

/// `%basalt_animation_<name>%`: the frame showing right now, taken from the wall clock so two
/// players see the same one and nothing is stored per player or ticked.
///
/// A frame may contain other placeholders, which is what lets an animation wrap live data.
/// Animations inside animations are refused *before* substituting: two frames naming each other
/// would recurse until the stack ran out.
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
