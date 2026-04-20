#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/EntityCommands.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using entity::AttributeModifier;
using entity::AttributeOperation;
using entity::ServerEntity;
using wire::common::Identifier;
using wire::text::Component;

[[nodiscard]] std::expected<ServerEntity*, CommandSyntaxError> entityOf(const CommandContext& context) {
    const auto* matched = context.get<std::vector<CommandTarget>>("target");
    ServerEntity* what = matched == nullptr || matched->size() != 1 ? nullptr : matched->front().entity();
    if (what == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::text(
                "A player's attributes are not modelled yet, so this only works on a summoned entity.")}};
    }
    return what;
}

[[nodiscard]] std::expected<std::string, CommandSyntaxError> attributeId(const CommandContext& context) {
    const auto* named = context.get<Identifier>("attribute");
    const std::string id = named == nullptr ? std::string{} : named->toString();
    if (!entity::attribute::defaultFor(id).has_value()) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("attribute.unknown", {Component::text(id)})}};
    }
    return id;
}

[[nodiscard]] std::string modifierIdOf(const CommandContext& context) {
    const auto* named = context.get<Identifier>("id");
    return named == nullptr ? std::string{} : named->toString();
}

[[nodiscard]] double scaleOf(const CommandContext& context) {
    const auto* scale = context.get<double>("scale");
    return scale == nullptr ? 1.0 : *scale;
}

[[nodiscard]] CommandSyntaxError noModifier(const std::string& attribute, const ServerEntity& entity,
                                            const std::string& id) {
    return CommandSyntaxError{
        .component = Component::translatable(
            "commands.attribute.failed.no_modifier",
            {Component::text(attribute), Component::text(std::string{entity.type().path()}), Component::text(id)})};
}

} // namespace

AttributeCommand::AttributeCommand() : Command("attribute", permissionLevel::kGamemasters) {}

void AttributeCommand::build(const CommandNodeBuilder& root) const {
    const auto read = [](const CommandContext& context, bool withModifiers) -> Parsed<std::int32_t> {
        const auto entity = entityOf(context);
        if (!entity.has_value()) {
            return std::unexpected{entity.error()};
        }
        const auto id = attributeId(context);
        if (!id.has_value()) {
            return std::unexpected{id.error()};
        }
        const double value =
            (withModifiers ? (*entity)->attributeWithModifiers(*id) : (*entity)->attribute(*id)) * scaleOf(context);
        context.success(Component::translatable(
            withModifiers ? "commands.attribute.value.get.success" : "commands.attribute.base_value.get.success",
            {Component::text(*id), Component::text(std::string{(*entity)->type().path()}),
             Component::text(std::to_string(value))}));
        return static_cast<std::int32_t>(value);
    };

    root.argument("target", entitiesArgument(true), [read](const CommandNodeBuilder& target) {
        target.argument(
            "attribute", resourceArgument("minecraft:attribute"), [read](const CommandNodeBuilder& attribute) {
                attribute.literal("get", [read](const CommandNodeBuilder& get) {
                    get.executes([read](const CommandContext& context) { return read(context, true); });
                    get.argument("scale", doubleArgument(0.0), [read](const CommandNodeBuilder& scale) {
                        scale.executes([read](const CommandContext& context) { return read(context, true); });
                    });
                });
                attribute.literal("base", [read](const CommandNodeBuilder& base) {
                    base.literal("get", [read](const CommandNodeBuilder& get) {
                        get.executes([read](const CommandContext& context) { return read(context, false); });
                        get.argument("scale", doubleArgument(0.0), [read](const CommandNodeBuilder& scale) {
                            scale.executes([read](const CommandContext& context) { return read(context, false); });
                        });
                    });
                    base.literal("set", [](const CommandNodeBuilder& set) {
                        set.argument("value", doubleArgument(), [](const CommandNodeBuilder& value) {
                            value.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                                const auto entity = entityOf(context);
                                if (!entity.has_value()) {
                                    return std::unexpected{entity.error()};
                                }
                                const auto id = attributeId(context);
                                if (!id.has_value()) {
                                    return std::unexpected{id.error()};
                                }
                                const auto* asked = context.get<double>("value");
                                if (asked == nullptr) {
                                    return 0;
                                }
                                (*entity)->attributeValues.insert_or_assign(*id, *asked);
                                if (*id == entity::attribute::kMaxHealth) {
                                    (*entity)->health = std::min((*entity)->health, *asked);
                                }
                                context.success(
                                    Component::translatable("commands.attribute.base_value.set.success",
                                                            {Component::text(*id),
                                                             Component::text(std::string{(*entity)->type().path()}),
                                                             Component::text(std::to_string(*asked))}),
                                    true);
                                return static_cast<std::int32_t>(*asked);
                            });
                        });
                    });
                    base.literal("reset", [](const CommandNodeBuilder& reset) {
                        reset.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                            const auto entity = entityOf(context);
                            if (!entity.has_value()) {
                                return std::unexpected{entity.error()};
                            }
                            const auto id = attributeId(context);
                            if (!id.has_value()) {
                                return std::unexpected{id.error()};
                            }
                            (*entity)->attributeValues.erase(*id);
                            const double value = (*entity)->attribute(*id);
                            context.success(
                                Component::translatable("commands.attribute.base_value.reset.success",
                                                        {Component::text(*id),
                                                         Component::text(std::string{(*entity)->type().path()}),
                                                         Component::text(std::to_string(value))}),
                                true);
                            return static_cast<std::int32_t>(value);
                        });
                    });
                });

                attribute.literal("modifier", [](const CommandNodeBuilder& modifier) {
                    const auto carried = [](const SuggestionContext& context) {
                        std::vector<std::string> named;
                        const auto* matched = context.argument<std::vector<CommandTarget>>("target");
                        const auto* attribute = context.argument<Identifier>("attribute");
                        ServerEntity* what =
                            matched == nullptr || matched->size() != 1 ? nullptr : matched->front().entity();
                        if (what != nullptr && attribute != nullptr) {
                            const auto found = what->attributeModifiers.find(attribute->toString());
                            if (found != what->attributeModifiers.end()) {
                                for (const auto& [id, one] : found->second) {
                                    named.push_back(id);
                                }
                            }
                        }
                        return context.matching(named);
                    };

                    modifier.literal("add", [](const CommandNodeBuilder& add) {
                        add.argument("id", resourceLocationArgument(), [](const CommandNodeBuilder& id) {
                            id.argument("value", doubleArgument(), [](const CommandNodeBuilder& value) {
                                for (const AttributeOperation operation :
                                     {AttributeOperation::AddValue, AttributeOperation::AddMultipliedBase,
                                      AttributeOperation::AddMultipliedTotal}) {
                                    value.literal(
                                        std::string{entity::attributeOperationName(operation)},
                                        [operation](const CommandNodeBuilder& one) {
                                            one.executes([operation](
                                                             const CommandContext& context) -> Parsed<std::int32_t> {
                                                const auto entity = entityOf(context);
                                                if (!entity.has_value()) {
                                                    return std::unexpected{entity.error()};
                                                }
                                                const auto attribute = attributeId(context);
                                                if (!attribute.has_value()) {
                                                    return std::unexpected{attribute.error()};
                                                }
                                                const std::string id = modifierIdOf(context);
                                                auto& modifiers = (*entity)->attributeModifiers[*attribute];
                                                if (modifiers.contains(id)) {
                                                    return std::unexpected{CommandSyntaxError{
                                                        .component = Component::translatable(
                                                            "commands.attribute.failed.modifier_"
                                                            "already_present",
                                                            {Component::text(id), Component::text(*attribute),
                                                             Component::text(std::string{(*entity)->type().path()})})}};
                                                }
                                                const auto* amount = context.get<double>("value");
                                                modifiers.insert_or_assign(
                                                    id, AttributeModifier{.id = id,
                                                                          .amount = amount == nullptr ? 0.0 : *amount,
                                                                          .operation = operation});
                                                context.success(
                                                    Component::translatable(
                                                        "commands.attribute.modifier.add.success",
                                                        {Component::text(id), Component::text(*attribute),
                                                         Component::text(std::string{(*entity)->type().path()})}),
                                                    true);
                                                return 1;
                                            });
                                        });
                                }
                            });
                        });
                    });

                    modifier.literal("remove", [carried](const CommandNodeBuilder& remove) {
                        remove.argument("id", resourceLocationArgument(), [carried](const CommandNodeBuilder& id) {
                            id.suggestsFromServer(carried);
                            id.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                                const auto entity = entityOf(context);
                                if (!entity.has_value()) {
                                    return std::unexpected{entity.error()};
                                }
                                const auto attribute = attributeId(context);
                                if (!attribute.has_value()) {
                                    return std::unexpected{attribute.error()};
                                }
                                const std::string id = modifierIdOf(context);
                                const auto found = (*entity)->attributeModifiers.find(*attribute);
                                if (found == (*entity)->attributeModifiers.end() || found->second.erase(id) == 0) {
                                    return std::unexpected{noModifier(*attribute, **entity, id)};
                                }
                                context.success(
                                    Component::translatable("commands.attribute.modifier.remove.success",
                                                            {Component::text(id), Component::text(*attribute),
                                                             Component::text(std::string{(*entity)->type().path()})}),
                                    true);
                                return 1;
                            });
                        });
                    });

                    modifier.literal("value", [carried](const CommandNodeBuilder& value) {
                        value.literal("get", [carried](const CommandNodeBuilder& get) {
                            get.argument("id", resourceLocationArgument(), [carried](const CommandNodeBuilder& id) {
                                id.suggestsFromServer(carried);
                                const auto report = [](const CommandContext& context) -> Parsed<std::int32_t> {
                                    const auto entity = entityOf(context);
                                    if (!entity.has_value()) {
                                        return std::unexpected{entity.error()};
                                    }
                                    const auto attribute = attributeId(context);
                                    if (!attribute.has_value()) {
                                        return std::unexpected{attribute.error()};
                                    }
                                    const std::string named = modifierIdOf(context);
                                    const auto found = (*entity)->attributeModifiers.find(*attribute);
                                    if (found == (*entity)->attributeModifiers.end()) {
                                        return std::unexpected{noModifier(*attribute, **entity, named)};
                                    }
                                    const auto one = found->second.find(named);
                                    if (one == found->second.end()) {
                                        return std::unexpected{noModifier(*attribute, **entity, named)};
                                    }
                                    const double amount = one->second.amount * scaleOf(context);
                                    context.success(
                                        Component::translatable("commands.attribute.modifier.value.get.success",
                                                                {Component::text(named), Component::text(*attribute),
                                                                 Component::text(std::string{(*entity)->type().path()}),
                                                                 Component::text(std::to_string(amount))}));
                                    return static_cast<std::int32_t>(amount);
                                };
                                id.executes(report);
                                id.argument("scale", doubleArgument(0.0),
                                            [report](const CommandNodeBuilder& scale) { scale.executes(report); });
                            });
                        });
                    });
                });
            });
    });
}

} // namespace basalt::server::runtime::command::commands
