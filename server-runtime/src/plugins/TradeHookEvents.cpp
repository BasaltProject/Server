#include <basalt/server/runtime/plugins/TradeHookEvents.hpp>

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <type_traits>

namespace basalt::server::runtime::plugins::tradeEvents {
namespace {

using server::plugins::PluginEvent;
using server::world::loot::ItemStack;

void setStack(PluginEvent& event, const std::string& item, const std::string& count, const ItemStack* stack) {
    event.set(item, stack == nullptr ? std::string{} : stack->item.toString());
    event.set(count, static_cast<std::int64_t>(stack == nullptr ? 0 : stack->count));
}

void applyStack(const PluginEvent& event, const std::string& item, const std::string& count,
                std::optional<ItemStack>& stack) {
    const auto* id = std::get_if<std::string>(&event.get(item));
    const auto* amount = std::get_if<std::int64_t>(&event.get(count));
    if (id == nullptr || amount == nullptr) {
        return;
    }
    if (id->empty() || *amount <= 0) {
        stack.reset();
        return;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
        wire::common::Identifier::parse(*id);
    if (!parsed.has_value()) {
        return;
    }
    if (!stack.has_value() || stack->item != *parsed) {
        stack = ItemStack{.item = *parsed};
    }
    stack->count = static_cast<std::int32_t>(*amount);
}

template <typename Value>
void applyNumber(const PluginEvent& event, const char* field, Value& target) {
    if constexpr (std::is_floating_point_v<Value>) {
        if (const auto* edited = std::get_if<double>(&event.get(field)); edited != nullptr) {
            target = static_cast<Value>(*edited);
        }
    } else if (const auto* edited = std::get_if<std::int64_t>(&event.get(field)); edited != nullptr) {
        target = static_cast<Value>(*edited);
    }
}

} // namespace

void fill(PluginEvent& event, const world::trades::Offer& offer) {
    setStack(event, "cost_a", "cost_a_count", &offer.costA);
    setStack(event, "cost_b", "cost_b_count", offer.costB.has_value() ? &*offer.costB : nullptr);
    setStack(event, "result", "result_count", &offer.result);
    event.set("uses", static_cast<std::int64_t>(offer.uses));
    event.set("max_uses", static_cast<std::int64_t>(offer.maxUses));
    event.set("experience", static_cast<std::int64_t>(offer.experience));
    event.set("price_multiplier", static_cast<double>(offer.priceMultiplier));
    event.set("demand", static_cast<std::int64_t>(offer.demand));
    event.set("special_price", static_cast<std::int64_t>(offer.specialPrice));
}

void apply(const PluginEvent& event, world::trades::Offer& offer) {
    std::optional<ItemStack> first = offer.costA;
    applyStack(event, "cost_a", "cost_a_count", first);
    if (first.has_value()) {
        offer.costA = std::move(*first);
    } else {
        offer.costA.count = 0;
    }
    applyStack(event, "cost_b", "cost_b_count", offer.costB);
    std::optional<ItemStack> result = offer.result;
    applyStack(event, "result", "result_count", result);
    if (result.has_value()) {
        offer.result = std::move(*result);
    }
    applyNumber(event, "uses", offer.uses);
    applyNumber(event, "max_uses", offer.maxUses);
    applyNumber(event, "experience", offer.experience);
    applyNumber(event, "price_multiplier", offer.priceMultiplier);
    applyNumber(event, "demand", offer.demand);
    applyNumber(event, "special_price", offer.specialPrice);
}

std::string linesOf(const std::vector<ItemStack>& stacks) {
    std::string lines;
    for (const ItemStack& stack : stacks) {
        lines += (lines.empty() ? "" : "\n") + stack.item.toString() + "|" + std::to_string(stack.count);
    }
    return lines;
}

std::vector<ItemStack> stacksOf(const std::string& lines) {
    std::vector<ItemStack> found;
    for (std::size_t from = 0; from < lines.size();) {
        const std::size_t end = std::min(lines.find('\n', from), lines.size());
        const std::string line = lines.substr(from, end - from);
        from = end + 1;
        const std::size_t bar = line.find('|');
        if (bar == std::string::npos) {
            continue;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(line.substr(0, bar));
        const std::int32_t count = std::atoi(line.c_str() + bar + 1);
        if (id.has_value() && count > 0) {
            found.push_back(ItemStack{.item = *id, .count = count});
        }
    }
    return found;
}

} // namespace basalt::server::runtime::plugins::tradeEvents
