#include <basalt/server/runtime/item/ItemLimits.hpp>
#include <basalt/wire/text/Serializers.hpp>

namespace basalt::server::runtime::item::itemLimits {
namespace dataComponent = wire::protocol::play::dataComponent;

std::int32_t lengthOf(std::string_view text) noexcept {
    std::int32_t characters = 0;
    for (const char c : text) {
        characters += (static_cast<unsigned char>(c) & 0xC0U) != 0x80U ? 1 : 0;
    }
    return characters;
}

std::string_view exceeded(std::span<const wire::protocol::play::ItemComponent> components,
                          const config::ItemCaps& caps) {
    for (const wire::protocol::play::ItemComponent& component : components) {
        if (const auto* text = component.as<wire::protocol::play::ItemComponent::Text>();
            text != nullptr && caps.customNameLength > 0 &&
            (component.id() == dataComponent::kCustomName || component.id() == dataComponent::kItemName) &&
            lengthOf(wire::text::toPlainText(text->text)) > caps.customNameLength) {
            return "custom-name-max-length";
        }
        if (const auto* lore = component.as<wire::protocol::play::ItemComponent::Lore>();
            lore != nullptr && caps.loreLineLength > 0) {
            for (const wire::text::Component& line : lore->lines) {
                if (lengthOf(wire::text::toPlainText(line)) > caps.loreLineLength) {
                    return "lore-line-max-length";
                }
            }
        }
    }
    return {};
}

} // namespace basalt::server::runtime::item::itemLimits
