#pragma once

#include <basalt/server/runtime/config/LimitsConfig.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <span>
#include <string_view>

namespace basalt::server::runtime::item::itemLimits {

/// Characters of text, not bytes.
[[nodiscard]] std::int32_t lengthOf(std::string_view text) noexcept;

/// The name of the first cap a client-supplied item breaks, empty when it fits them all.
[[nodiscard]] std::string_view exceeded(std::span<const wire::protocol::play::ItemComponent> components,
                                        const config::ItemCaps& caps);

} // namespace basalt::server::runtime::item::itemLimits
