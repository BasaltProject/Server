#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/MessageArguments.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::common::Identifier;
using wire::text::Component;

[[nodiscard]] std::string joined(const std::set<std::string>& names) {
    std::string text;
    for (const std::string& name : names) {
        text += text.empty() ? name : ", " + name;
    }
    return text;
}

[[nodiscard]] CommandSyntaxError unchanged(std::string_view what) {
    return CommandSyntaxError{.component =
                                  Component::translatable("commands.bossbar.set." + std::string{what} + ".unchanged")};
}

/// parity: `BossBarColor`'s own names, which are what the command's literals are.
[[nodiscard]] std::string_view bossBarColorName(play::BossBarColor color) {
    switch (color) {
    case play::BossBarColor::Pink:
        return "pink";
    case play::BossBarColor::Blue:
        return "blue";
    case play::BossBarColor::Red:
        return "red";
    case play::BossBarColor::Green:
        return "green";
    case play::BossBarColor::Yellow:
        return "yellow";
    case play::BossBarColor::Purple:
        return "purple";
    case play::BossBarColor::White:
    default:
        return "white";
    }
}

} // namespace

wire::nbt::NbtCompound BossBarRegistry::encode() const {
    wire::nbt::NbtCompound out;
    for (const wire::common::Identifier& id : order_) {
        const CustomBossBar& bar = bars_.at(id);
        wire::nbt::NbtCompound entry;
        entry.set("Name", wire::text::componentNbt::serialize(bar.name));
        entry.putBoolean("Visible", bar.visible);
        entry.putInt("Value", bar.value);
        entry.putInt("Max", bar.max);
        entry.putString("Color", std::string{bossBarColorName(bar.color)});
        entry.putString("Overlay", std::string{play::bossBarOverlayName(bar.overlay)});
        entry.putBoolean("DarkenScreen", bar.darkenScreen);
        entry.putBoolean("PlayBossMusic", bar.playMusic);
        entry.putBoolean("CreateWorldFog", bar.createFog);
        std::vector<wire::nbt::NbtTag> names;
        for (const std::string& name : bar.players) {
            names.push_back(wire::nbt::NbtTag::ofString(name));
        }
        entry.set("PlayerNames",
                  wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::String, std::move(names)}));
        out.set(id.toString(), wire::nbt::NbtTag::ofCompound(std::move(entry)));
    }
    return out;
}

void BossBarRegistry::restore(const wire::nbt::NbtCompound& packed) {
    for (std::size_t i = 0; i < packed.size(); ++i) {
        const wire::nbt::NbtCompound* entry = packed.values()[i].get<wire::nbt::NbtCompound>();
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(packed.names()[i]);
        if (entry == nullptr || !id.has_value()) {
            continue;
        }
        CustomBossBar bar{.id = *id};
        if (const wire::nbt::NbtTag* name = entry->find("Name"); name != nullptr) {
            bar.name = wire::text::componentNbt::deserialize(*name);
        }
        bar.visible = entry->getBoolean("Visible", false);
        bar.value = entry->getInt("Value", 0);
        bar.max = entry->getInt("Max", 100);
        bar.darkenScreen = entry->getBoolean("DarkenScreen", false);
        bar.playMusic = entry->getBoolean("PlayBossMusic", false);
        bar.createFog = entry->getBoolean("CreateWorldFog", false);
        for (const play::BossBarColor color :
             {play::BossBarColor::Pink, play::BossBarColor::Blue, play::BossBarColor::Red, play::BossBarColor::Green,
              play::BossBarColor::Yellow, play::BossBarColor::Purple, play::BossBarColor::White}) {
            if (bossBarColorName(color) == entry->getString("Color")) {
                bar.color = color;
            }
        }
        for (const play::BossBarOverlay overlay :
             {play::BossBarOverlay::Progress, play::BossBarOverlay::Notched6, play::BossBarOverlay::Notched10,
              play::BossBarOverlay::Notched12, play::BossBarOverlay::Notched20}) {
            if (play::bossBarOverlayName(overlay) == entry->getString("Overlay")) {
                bar.overlay = overlay;
            }
        }
        if (const wire::nbt::NbtList* names = entry->getList("PlayerNames"); names != nullptr) {
            for (const wire::nbt::NbtTag& name : *names) {
                if (const std::string* text = name.get<std::string>(); text != nullptr) {
                    bar.players.insert(*text);
                }
            }
        }
        add(std::move(bar));
    }
}

float CustomBossBar::progress() const {
    return max == 0 ? 0.0F : std::clamp(static_cast<float>(value) / static_cast<float>(max), 0.0F, 1.0F);
}

play::BossEventPacket::Add CustomBossBar::addOperation() const {
    return play::BossEventPacket::Add{.name = name,
                                      .progress = progress(),
                                      .color = color,
                                      .overlay = overlay,
                                      .darkenScreen = darkenScreen,
                                      .playMusic = playMusic,
                                      .createWorldFog = createFog};
}

std::vector<CustomBossBar*> BossBarRegistry::all() {
    std::vector<CustomBossBar*> found;
    found.reserve(order_.size());
    for (const Identifier& id : order_) {
        found.push_back(&bars_.at(id));
    }
    return found;
}

std::vector<const CustomBossBar*> BossBarRegistry::all() const {
    std::vector<const CustomBossBar*> found;
    found.reserve(order_.size());
    for (const Identifier& id : order_) {
        found.push_back(&bars_.at(id));
    }
    return found;
}

CustomBossBar* BossBarRegistry::get(const Identifier& id) {
    const auto found = bars_.find(id);
    return found == bars_.end() ? nullptr : &found->second;
}

void BossBarRegistry::add(CustomBossBar bar) {
    if (!bars_.contains(bar.id)) {
        order_.push_back(bar.id);
    }
    const Identifier id = bar.id;
    bars_.insert_or_assign(id, std::move(bar));
}

bool BossBarRegistry::remove(const Identifier& id) {
    // Copied first: the caller's reference is usually the bar's own field, which erasing destroys.
    const Identifier wanted = id;
    if (bars_.erase(wanted) == 0) {
        return false;
    }
    std::erase(order_, wanted);
    return true;
}

std::vector<const CustomBossBar*> BossBarRegistry::barsFor(const player::PlayerSession& player) const {
    std::vector<const CustomBossBar*> found;
    for (const CustomBossBar* bar : all()) {
        if (bar->visible && bar->players.contains(player.name())) {
            found.push_back(bar);
        }
    }
    return found;
}

BossBarCommand::BossBarCommand(BossBarRegistry& registry)
    : Command("bossbar", permissionLevel::kGamemasters), registry_(&registry) {}

void BossBarCommand::build(const CommandNodeBuilder& root) const {
    const auto ids = [this](const SuggestionContext& context) {
        std::vector<Identifier> named;
        for (const CustomBossBar* bar : std::as_const(*registry_).all()) {
            named.push_back(bar->id);
        }
        return context.matchingIds(named);
    };
    const auto barOf = [this](const CommandContext& context) -> std::expected<CustomBossBar*, CommandSyntaxError> {
        const auto* id = context.get<Identifier>("id");
        CustomBossBar* bar = id == nullptr ? nullptr : registry_->get(*id);
        if (bar == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable(
                    "commands.bossbar.unknown", {Component::text(id == nullptr ? std::string{} : id->toString())})}};
        }
        return bar;
    };
    const auto viewers = [](const CommandContext& context, const CustomBossBar& bar) {
        std::vector<player::PlayerSession*> found;
        for (player::PlayerSession* player : context.environment().players().snapshot()) {
            if (bar.players.contains(player->name())) {
                found.push_back(player);
            }
        }
        return found;
    };
    const auto send = [](const player::PlayerSession& player, const play::BossEventPacket& packet) {
        if (player.connection().showBossBar) {
            if (const auto* add = std::get_if<play::BossEventPacket::Add>(&packet.operation); add != nullptr) {
                player.connection().showBossBar(packet.id, add->name, add->progress, true);
                return;
            }
            if (const auto* named = std::get_if<play::BossEventPacket::UpdateName>(&packet.operation);
                named != nullptr) {
                player.connection().showBossBar(packet.id, named->name, 1.0F, true);
                return;
            }
            if (const auto* progress = std::get_if<play::BossEventPacket::UpdateProgress>(&packet.operation);
                progress != nullptr) {
                player.connection().showBossBar(packet.id, Component::empty(), progress->progress, true);
                return;
            }
            if (std::holds_alternative<play::BossEventPacket::Remove>(packet.operation)) {
                player.connection().showBossBar(packet.id, Component::empty(), 0.0F, false);
            }
            return;
        }
        if (player.connection().sendPlayPacket) {
            player.connection().sendPlayPacket(play::BossEventPacket::kId,
                                               [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    };
    const auto broadcast = [viewers, send](const CommandContext& context, const CustomBossBar& bar,
                                           const play::BossEventPacket::Operation& operation) {
        if (!bar.visible) {
            return;
        }
        const play::BossEventPacket packet{.id = bar.uuid, .operation = operation};
        for (const player::PlayerSession* player : viewers(context, bar)) {
            send(*player, packet);
        }
    };

    root.literal("add", [this](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [this](const CommandNodeBuilder& id) {
            id.argument("name", componentArgument(), [this](const CommandNodeBuilder& name) {
                name.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto* wanted = context.get<Identifier>("id");
                    const auto* label = command::messageArguments::resolvedComponent(context, "name");
                    if (wanted == nullptr || label == nullptr) {
                        return 0;
                    }
                    if (registry_->get(*wanted) != nullptr) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.bossbar.create.failed",
                                                                 {Component::text(wanted->toString())})}};
                    }
                    registry_->add(CustomBossBar{.id = *wanted, .name = *label});
                    context.success(Component::translatable("commands.bossbar.create.success", {*label}), true);
                    return static_cast<std::int32_t>(registry_->all().size());
                });
            });
        });
    });

    root.literal("remove", [this, ids, barOf, viewers, send](const CommandNodeBuilder& form) {
        form.argument(
            "id", resourceLocationArgument(), [this, ids, barOf, viewers, send](const CommandNodeBuilder& id) {
                id.suggestsFromServer(ids);
                id.executes([this, barOf, viewers, send](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto bar = barOf(context);
                    if (!bar.has_value()) {
                        return std::unexpected{bar.error()};
                    }
                    const play::BossEventPacket packet{.id = (*bar)->uuid,
                                                       .operation = play::BossEventPacket::Remove{}};
                    for (const player::PlayerSession* player : viewers(context, **bar)) {
                        send(*player, packet);
                    }
                    const Component name = (*bar)->name;
                    registry_->remove((*bar)->id);
                    context.success(Component::translatable("commands.bossbar.remove.success", {name}), true);
                    return static_cast<std::int32_t>(registry_->all().size());
                });
            });
    });

    root.literal("list", [this](const CommandNodeBuilder& form) {
        form.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const std::vector<const CustomBossBar*> bars = std::as_const(*registry_).all();
            std::string named;
            for (const CustomBossBar* bar : bars) {
                named += named.empty() ? bar->id.toString() : ", " + bar->id.toString();
            }
            context.success(bars.empty() ? Component::translatable("commands.bossbar.list.bars.none")
                                         : Component::translatable(
                                               "commands.bossbar.list.bars.some",
                                               {Component::text(std::to_string(bars.size())), Component::text(named)}));
            return static_cast<std::int32_t>(bars.size());
        });
    });

    root.literal("get", [ids, barOf](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [ids, barOf](const CommandNodeBuilder& id) {
            id.suggestsFromServer(ids);
            for (const std::string_view what : {"value", "max", "visible", "players"}) {
                id.literal(std::string{what}, [what, barOf](const CommandNodeBuilder& form) {
                    form.executes([what, barOf](const CommandContext& context) -> Parsed<std::int32_t> {
                        const auto found = barOf(context);
                        if (!found.has_value()) {
                            return std::unexpected{found.error()};
                        }
                        const CustomBossBar& bar = **found;
                        if (what == "value") {
                            context.success(Component::translatable(
                                "commands.bossbar.get.value", {bar.name, Component::text(std::to_string(bar.value))}));
                            return bar.value;
                        }
                        if (what == "max") {
                            context.success(Component::translatable(
                                "commands.bossbar.get.max", {bar.name, Component::text(std::to_string(bar.max))}));
                            return bar.max;
                        }
                        if (what == "visible") {
                            context.success(Component::translatable(bar.visible ? "commands.bossbar.get.visible.visible"
                                                                                : "commands.bossbar.get.visible.hidden",
                                                                    {bar.name}));
                            return bar.visible ? 1 : 0;
                        }
                        context.success(bar.players.empty()
                                            ? Component::translatable("commands.bossbar.get.players.none", {bar.name})
                                            : Component::translatable(
                                                  "commands.bossbar.get.players.some",
                                                  {bar.name, Component::text(std::to_string(bar.players.size())),
                                                   Component::text(joined(bar.players))}));
                        return static_cast<std::int32_t>(bar.players.size());
                    });
                });
            }
        });
    });

    root.literal("set", [ids, barOf, viewers, send, broadcast](const CommandNodeBuilder& form) {
        form.argument(
            "id", resourceLocationArgument(), [ids, barOf, viewers, send, broadcast](const CommandNodeBuilder& id) {
                id.suggestsFromServer(ids);
                id.literal("name", [barOf, broadcast](const CommandNodeBuilder& name) {
                    name.argument("name", componentArgument(), [barOf, broadcast](const CommandNodeBuilder& label) {
                        label.executes([barOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                            const auto found = barOf(context);
                            if (!found.has_value()) {
                                return std::unexpected{found.error()};
                            }
                            const auto* wanted = command::messageArguments::resolvedComponent(context, "name");
                            if (wanted == nullptr || (*found)->name == *wanted) {
                                return std::unexpected{unchanged("name")};
                            }
                            (*found)->name = *wanted;
                            broadcast(context, **found, play::BossEventPacket::UpdateName{.name = *wanted});
                            context.success(
                                Component::translatable("commands.bossbar.set.name.success", {(*found)->name}), true);
                            return 0;
                        });
                    });
                });
                id.literal("color", [barOf, broadcast](const CommandNodeBuilder& colours) {
                    for (std::uint8_t ordinal = 0; ordinal <= static_cast<std::uint8_t>(play::BossBarColor::White);
                         ordinal++) {
                        const auto colour = static_cast<play::BossBarColor>(ordinal);
                        colours.literal(std::string{bossBarColorName(colour)}, [colour, barOf, broadcast](
                                                                                   const CommandNodeBuilder& one) {
                            one.executes(
                                [colour, barOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                                    const auto found = barOf(context);
                                    if (!found.has_value()) {
                                        return std::unexpected{found.error()};
                                    }
                                    if ((*found)->color == colour) {
                                        return std::unexpected{unchanged("color")};
                                    }
                                    (*found)->color = colour;
                                    broadcast(context, **found,
                                              play::BossEventPacket::UpdateStyle{.color = colour,
                                                                                 .overlay = (*found)->overlay});
                                    context.success(
                                        Component::translatable("commands.bossbar.set.color.success", {(*found)->name}),
                                        true);
                                    return 0;
                                });
                        });
                    }
                });
                id.literal("style", [barOf, broadcast](const CommandNodeBuilder& styles) {
                    for (std::uint8_t ordinal = 0;
                         ordinal <= static_cast<std::uint8_t>(play::BossBarOverlay::Notched20); ordinal++) {
                        const auto overlay = static_cast<play::BossBarOverlay>(ordinal);
                        styles.literal(
                            std::string{play::bossBarOverlayName(overlay)},
                            [overlay, barOf, broadcast](const CommandNodeBuilder& one) {
                                one.executes(
                                    [overlay, barOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                                        const auto found = barOf(context);
                                        if (!found.has_value()) {
                                            return std::unexpected{found.error()};
                                        }
                                        if ((*found)->overlay == overlay) {
                                            return std::unexpected{unchanged("style")};
                                        }
                                        (*found)->overlay = overlay;
                                        broadcast(context, **found,
                                                  play::BossEventPacket::UpdateStyle{.color = (*found)->color,
                                                                                     .overlay = overlay});
                                        context.success(Component::translatable("commands.bossbar.set.style.success",
                                                                                {(*found)->name}),
                                                        true);
                                        return 0;
                                    });
                            });
                    }
                });
                for (const bool isValue : {true, false}) {
                    const std::string what = isValue ? "value" : "max";
                    id.literal(what, [isValue, what, barOf, broadcast](const CommandNodeBuilder& form) {
                        form.argument(
                            what, integerArgument(isValue ? 0 : 1),
                            [isValue, what, barOf, broadcast](const CommandNodeBuilder& number) {
                                number.executes([isValue, what, barOf,
                                                 broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                                    const auto found = barOf(context);
                                    if (!found.has_value()) {
                                        return std::unexpected{found.error()};
                                    }
                                    const auto* asked = context.get<std::int32_t>(what);
                                    if (asked == nullptr) {
                                        return 0;
                                    }
                                    std::int32_t& field = isValue ? (*found)->value : (*found)->max;
                                    if (field == *asked) {
                                        return std::unexpected{unchanged(what)};
                                    }
                                    field = *asked;
                                    broadcast(context, **found,
                                              play::BossEventPacket::UpdateProgress{.progress = (*found)->progress()});
                                    context.success(Component::translatable(
                                                        "commands.bossbar.set." + what + ".success",
                                                        {(*found)->name, Component::text(std::to_string(*asked))}),
                                                    true);
                                    return *asked;
                                });
                            });
                    });
                }
                id.literal("visible", [barOf, viewers, send](const CommandNodeBuilder& form) {
                    form.argument("visible", boolArgument(), [barOf, viewers, send](const CommandNodeBuilder& flag) {
                        flag.executes([barOf, viewers, send](const CommandContext& context) -> Parsed<std::int32_t> {
                            const auto found = barOf(context);
                            if (!found.has_value()) {
                                return std::unexpected{found.error()};
                            }
                            const auto* wanted = context.get<bool>("visible");
                            if (wanted == nullptr) {
                                return 0;
                            }
                            CustomBossBar& bar = **found;
                            if (bar.visible == *wanted) {
                                return std::unexpected{CommandSyntaxError{
                                    .component =
                                        Component::translatable(*wanted ? "commands.bossbar.set.visibility.unchanged."
                                                                          "visible"
                                                                        : "commands.bossbar.set.visibility.unchanged."
                                                                          "hidden")}};
                            }
                            bar.visible = *wanted;
                            const play::BossEventPacket packet{
                                .id = bar.uuid,
                                .operation = *wanted
                                                 ? play::BossEventPacket::Operation{bar.addOperation()}
                                                 : play::BossEventPacket::Operation{play::BossEventPacket::Remove{}}};
                            for (const player::PlayerSession* player : viewers(context, bar)) {
                                send(*player, packet);
                            }
                            context.success(Component::translatable(*wanted
                                                                        ? "commands.bossbar.set.visible.success.visible"
                                                                        : "commands.bossbar.set.visible.success.hidden",
                                                                    {bar.name}),
                                            true);
                            return 0;
                        });
                    });
                });
                id.literal("players", [barOf, send](const CommandNodeBuilder& form) {
                    const auto setPlayers = [barOf, send](const CommandContext& context,
                                                          bool named) -> Parsed<std::int32_t> {
                        const auto found = barOf(context);
                        if (!found.has_value()) {
                            return std::unexpected{found.error()};
                        }
                        CustomBossBar& bar = **found;
                        const auto* matched =
                            named ? context.get<std::vector<player::PlayerSession*>>("targets") : nullptr;
                        std::set<std::string> names;
                        if (matched != nullptr) {
                            for (const player::PlayerSession* one : *matched) {
                                names.insert(one->name());
                            }
                        }
                        if (bar.players == names) {
                            return std::unexpected{unchanged("players")};
                        }
                        // Whoever is dropping off the bar loses it now, before the set changes underneath them.
                        const play::BossEventPacket removal{.id = bar.uuid,
                                                            .operation = play::BossEventPacket::Remove{}};
                        for (const player::PlayerSession* player : context.environment().players().snapshot()) {
                            if (bar.players.contains(player->name()) && !names.contains(player->name())) {
                                send(*player, removal);
                            }
                        }
                        std::vector<player::PlayerSession*> added;
                        if (matched != nullptr) {
                            for (player::PlayerSession* one : *matched) {
                                if (!bar.players.contains(one->name())) {
                                    added.push_back(one);
                                }
                            }
                        }
                        bar.players = names;
                        if (bar.visible) {
                            const play::BossEventPacket packet{.id = bar.uuid, .operation = bar.addOperation()};
                            for (const player::PlayerSession* one : added) {
                                send(*one, packet);
                            }
                        }
                        context.success(
                            names.empty()
                                ? Component::translatable("commands.bossbar.set.players.success.none", {bar.name})
                                : Component::translatable("commands.bossbar.set.players.success.some",
                                                          {bar.name, Component::text(std::to_string(names.size())),
                                                           Component::text(joined(names))}),
                            true);
                        return static_cast<std::int32_t>(names.size());
                    };
                    form.executes([setPlayers](const CommandContext& context) { return setPlayers(context, false); });
                    form.argument("targets", optionalPlayersArgument(),
                                  [setPlayers](const CommandNodeBuilder& targets) {
                                      targets.executes([setPlayers](const CommandContext& context) {
                                          return setPlayers(context, true);
                                      });
                                  });
                });
            });
    });
}

} // namespace basalt::server::runtime::command::commands
