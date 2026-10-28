#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PacketNames.hpp>
#include <basalt/server/runtime/plugins/ServerPluginServices.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

namespace basalt::server::runtime::plugins {
namespace {

using server::plugins::PluginArgumentType;
using server::plugins::PluginCommandArgument;
using server::plugins::PluginCommandCall;
using server::plugins::PluginCommandSpec;
using server::plugins::PluginHandle;

class NoPlayers final : public server::plugins::PluginPlayers {
public:
    [[nodiscard]] std::vector<std::shared_ptr<server::plugins::PluginPlayerRef>> online() const override { return {}; }

    [[nodiscard]] std::shared_ptr<server::plugins::PluginPlayerRef> byName(std::string_view) const override {
        return nullptr;
    }

    [[nodiscard]] std::shared_ptr<server::plugins::PluginPlayerRef> byUuid(std::string_view) const override {
        return nullptr;
    }
};

class NoWorlds final : public server::plugins::PluginWorlds {
public:
    [[nodiscard]] std::vector<std::shared_ptr<server::plugins::PluginWorldRef>> all() const override { return {}; }

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> byName(std::string_view) const override {
        return nullptr;
    }

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> defaultWorld() const override { return nullptr; }

    std::shared_ptr<server::plugins::PluginWorldRef> create(std::string_view, std::string_view, std::string_view,
                                                            std::optional<std::int64_t>) override {
        return nullptr;
    }
};

class NoPlaceholders final : public server::plugins::PluginPlaceholders {
public:
    PluginHandle registerPlaceholder(std::string_view, std::string_view, Value) override { return {}; }

    [[nodiscard]] std::string fill(std::string_view text, server::plugins::PluginPlayerRef*) const override {
        return std::string{text};
    }
};

class NoStorage final : public server::plugins::PluginStorage {
public:
    [[nodiscard]] std::optional<std::string> playerValue(std::string_view, std::string_view,
                                                         std::string_view) override {
        return std::nullopt;
    }

    void setPlayerValue(std::string_view, std::string_view, std::string_view,
                        const std::optional<std::string>&) override {}

    [[nodiscard]] std::map<std::string, std::string> playerValues(std::string_view, std::string_view) override {
        return {};
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> top(std::string_view, std::string_view,
                                                                       std::int32_t) override {
        return {};
    }
};

class NoMenus final : public server::plugins::PluginMenus {
public:
    PluginHandle open(server::plugins::PluginPlayerRef&, std::string_view, std::int32_t,
                      const std::map<std::int32_t, server::plugins::PluginItem>&,
                      std::function<void(std::int32_t)>) override {
        return {};
    }

    void close(server::plugins::PluginPlayerRef&) override {}

    void refresh(server::plugins::PluginPlayerRef&,
                 const std::map<std::int32_t, server::plugins::PluginItem>&) override {}
};

class NoPackets final : public NamedPackets {
public:
    bool send(server::plugins::PluginPlayerRef&, const server::plugins::PacketAddress&, std::span<const std::uint8_t>,
              bool) override {
        return false;
    }

    bool receive(server::plugins::PluginPlayerRef&, const server::plugins::PacketAddress&,
                 std::span<const std::uint8_t>, bool) override {
        return false;
    }

    [[nodiscard]] std::shared_ptr<server::plugins::PacketEndpoint>
    endpointOf(server::plugins::PluginPlayerRef&) override {
        return nullptr;
    }
};

class NoChannels final : public server::plugins::PluginChannelSender {
public:
    bool send(server::plugins::PluginPlayerRef&, std::string_view, std::span<const std::uint8_t>) override {
        return false;
    }
};

NoPlayers& noPlayers() {
    static NoPlayers kNone;
    return kNone;
}

NoWorlds& noWorlds() {
    static NoWorlds kNone;
    return kNone;
}

NoPlaceholders& noPlaceholders() {
    static NoPlaceholders kNone;
    return kNone;
}

NoStorage& noStorage() {
    static NoStorage kNone;
    return kNone;
}

NoMenus& noMenus() {
    static NoMenus kNone;
    return kNone;
}

NoPackets& noPackets() {
    static NoPackets kNone;
    return kNone;
}

NoChannels& noChannels() {
    static NoChannels kNone;
    return kNone;
}

class DispatchedCall final : public PluginCommandCall {
public:
    struct Origin {
        std::string kind;
        std::string world;
        std::array<double, 3> position{};
        std::string caller;
    };

    DispatchedCall(std::string source, bool console, std::map<std::string, Argument, std::less<>> values,
                   std::function<void(const wire::text::Component&)> reply, Origin origin)
        : source_(std::move(source)), console_(console), values_(std::move(values)), reply_(std::move(reply)),
          origin_(std::move(origin)) {}

    [[nodiscard]] std::string source() const override { return source_; }

    [[nodiscard]] bool isConsole() const override { return console_; }

    [[nodiscard]] std::string kind() const override {
        return origin_.kind.empty() ? (console_ ? "console" : "player") : origin_.kind;
    }

    [[nodiscard]] std::string world() const override { return origin_.world; }

    [[nodiscard]] std::array<double, 3> position() const override { return origin_.position; }

    [[nodiscard]] std::string caller() const override { return origin_.caller; }

    void reply(std::string_view message) override {
        if (reply_) {
            reply_(wire::text::miniMessage::parse(std::string{message}));
        }
    }

    [[nodiscard]] Argument argument(std::string_view name) const override {
        const auto found = values_.find(name);
        return found == values_.end() ? Argument{} : found->second;
    }

private:
    std::string source_;
    bool console_;
    std::map<std::string, Argument, std::less<>> values_;
    std::function<void(const wire::text::Component&)> reply_;
    Origin origin_;
};

[[nodiscard]] std::shared_ptr<const command::ArgumentType> typeOf(const PluginCommandArgument& argument) {
    switch (argument.type) {
    case PluginArgumentType::Word:
        return command::wordArgument();
    case PluginArgumentType::Text:
        return command::greedyStringArgument();
    case PluginArgumentType::Int:
        return command::integerArgument(
            static_cast<std::int32_t>(argument.min.value_or(std::numeric_limits<std::int32_t>::min())),
            static_cast<std::int32_t>(argument.max.value_or(std::numeric_limits<std::int32_t>::max())));
    case PluginArgumentType::Float:
        return command::doubleArgument(
            argument.min.has_value() ? static_cast<double>(*argument.min) : -std::numeric_limits<double>::max(),
            argument.max.has_value() ? static_cast<double>(*argument.max) : std::numeric_limits<double>::max());
    case PluginArgumentType::Bool:
        return command::boolArgument();
    }
    return command::wordArgument();
}

} // namespace

PluginHandle OwnerTasks::after(std::int32_t delayTicks, std::function<void()> task) {
    sched::TaskHandle handle = owner_->schedule(delayTicks, std::move(task));
    return PluginHandle{[handle]() mutable { handle.cancel(); }};
}

PluginHandle OwnerTasks::every(std::int32_t delayTicks, std::int32_t periodTicks, std::function<void()> task) {
    const std::int32_t period = std::max(periodTicks, 1);
    // Shared because the re-arm outlives the call that made it, and cancelling has to reach both.
    const auto cancelled = std::make_shared<bool>(false);
    const auto handle = std::make_shared<sched::TaskHandle>();
    const auto held = std::make_shared<std::function<void()>>(std::move(task));
    sched::Owner* owner = owner_;
    // The re-arm asks one turn less than the period: a task scheduled mid-turn is due that many turns later.
    const auto arm = std::make_shared<std::function<void(std::int32_t)>>();
    *arm = [owner, cancelled, handle, held, arm, period](std::int32_t delay) {
        *handle = owner->schedule(delay, [cancelled, held, arm, period] {
            if (*cancelled) {
                return;
            }
            (*held)();
            if (!*cancelled) {
                (*arm)(period - 1);
            }
        });
    };
    (*arm)(std::max(delayTicks, 0));
    return PluginHandle{[cancelled, handle] {
        *cancelled = true;
        handle->cancel();
    }};
}

command::CommandNode* DispatcherCommands::add(Entry& entry) {
    Entry* target = &entry;
    DispatcherCommands* self = this;
    sched::Owner* owner = owner_;
    return &dispatcher_->registerNode(entry.spec.name(), [target, owner,
                                                          self](const command::CommandNodeBuilder& root) {
        // An empty permission opens the command to everyone.
        if (!target->spec.permission.has_value() || !target->spec.permission->empty()) {
            root.permission(
                target->spec.permission.value_or("basalt.plugin." + target->pluginId + "." + target->spec.name()));
        }
        const auto arguments = std::make_shared<std::function<void(const command::CommandNodeBuilder&, std::size_t)>>();
        *arguments = [target, owner, arguments, self](const command::CommandNodeBuilder& builder, std::size_t index) {
            // An optional argument makes the command runnable without it, so every node from it onwards executes.
            const bool runnable = index >= target->spec.arguments.size() || target->spec.arguments[index].optional;
            if (runnable) {
                builder.executes([target, owner, index, self](const command::CommandContext& context) {
                    std::map<std::string, PluginCommandCall::Argument, std::less<>> values;
                    for (std::size_t at = 0; at < index && at < target->spec.arguments.size(); at++) {
                        const PluginCommandArgument& argument = target->spec.arguments[at];
                        if (!context.has(argument.name)) {
                            continue;
                        }
                        if (const auto* text = context.get<std::string>(argument.name); text != nullptr) {
                            values.emplace(argument.name, *text);
                        } else if (const auto* whole = context.get<std::int32_t>(argument.name); whole != nullptr) {
                            values.emplace(argument.name, static_cast<std::int64_t>(*whole));
                        } else if (const auto* real = context.get<double>(argument.name); real != nullptr) {
                            values.emplace(argument.name, *real);
                        } else if (const auto* flag = context.get<bool>(argument.name); flag != nullptr) {
                            values.emplace(argument.name, *flag);
                        }
                    }
                    // Handed to the owner: plugin code only ever runs on one, and the command arrives on the source's.
                    const command::CommandSource& source = context.source();
                    DispatchedCall::Origin origin{.kind = std::string{source.kind()}, .position = source.position()};
                    if (const world::LoadedWorld* at = command::worldIn(source, context.environment()); at != nullptr) {
                        origin.world = at->name();
                    }
                    if (const command::ExecutionSource* execution = source.asExecution(); execution != nullptr) {
                        origin.caller = std::string{execution->root().kind()} + ":" + execution->root().name();
                    }
                    const bool inline_ = owner->isCurrent();
                    std::function<void(const wire::text::Component&)> reply;
                    if (inline_) {
                        reply = [&source](const wire::text::Component& message) { source.sendSystemMessage(message); };
                    } else if (source.player() != nullptr) {
                        reply = [self, uuid = source.player()->uuid().toString()](const wire::text::Component& m) {
                            if (self->playerReplier_) {
                                self->playerReplier_(uuid, m);
                            }
                        };
                    } else {
                        reply = [self](const wire::text::Component& message) {
                            if (const command::CommandSource* console = self->console_ ? self->console_() : nullptr;
                                console != nullptr) {
                                console->sendSystemMessage(message);
                            }
                        };
                    }
                    auto call =
                        std::make_shared<DispatchedCall>(source.name(), source.player() == nullptr, std::move(values),
                                                         std::move(reply), std::move(origin));
                    const std::function<void(PluginCommandCall&)> run = target->run;
                    if (inline_) {
                        run(*call);
                    } else {
                        static_cast<void>(owner->submit([run, call] { run(*call); }));
                    }
                    return 1;
                });
            }
            if (index >= target->spec.arguments.size()) {
                return;
            }
            const PluginCommandArgument& argument = target->spec.arguments[index];
            static_cast<void>(builder.argument(
                argument.name, typeOf(argument),
                [arguments, index](const command::CommandNodeBuilder& next) { (*arguments)(next, index + 1); }));
        };
        const auto literals = std::make_shared<std::function<void(const command::CommandNodeBuilder&, std::size_t)>>();
        *literals = [target, literals, arguments](const command::CommandNodeBuilder& builder, std::size_t index) {
            if (index < target->spec.literals.size()) {
                static_cast<void>(builder.literal(
                    target->spec.literals[index],
                    [literals, index](const command::CommandNodeBuilder& next) { (*literals)(next, index + 1); }));
                return;
            }
            (*arguments)(builder, 0);
        };
        (*literals)(root, 1);
    });
}

PluginHandle DispatcherCommands::registerCommand(std::string_view pluginId, const PluginCommandSpec& spec,
                                                 std::function<void(PluginCommandCall&)> run) {
    auto entry = std::make_unique<Entry>(
        Entry{.pluginId = std::string{pluginId}, .spec = spec, .run = std::move(run), .node = nullptr});
    Entry* target = entry.get();
    entries_.push_back(std::move(entry));
    target->node = add(*target);
    command::CommandDispatcher* dispatcher = dispatcher_;
    return PluginHandle{[this, dispatcher, target] {
        if (target->node != nullptr) {
            dispatcher->unregisterNode(target->node);
        }
        std::erase_if(entries_, [target](const std::unique_ptr<Entry>& held) { return held.get() == target; });
    }};
}

bool DispatcherCommands::runAsConsole(std::string_view command) {
    const command::CommandSource* source = console_ ? console_() : nullptr;
    if (source == nullptr) {
        return false;
    }
    std::string line{command};
    if (line.starts_with('/')) {
        line.erase(0, 1);
    }
    static_cast<void>(dispatcher_->execute(*source, line));
    return true;
}

bool DispatcherCommands::runAsPlayer(server::plugins::PluginPlayerRef& player, std::string_view command) {
    if (!playerRunner_) {
        return false;
    }
    std::string line{command};
    if (line.starts_with('/')) {
        line.erase(0, 1);
    }
    return playerRunner_(player.uuid(), line);
}

void DispatcherCommands::refresh() {
    for (const std::unique_ptr<Entry>& entry : entries_) {
        entry->node = add(*entry);
    }
}

RootPaths::RootPaths(std::string root) : packages_(std::move(root) + "/plugins") {
    if (packages_.starts_with("./")) {
        packages_.erase(0, 2);
    }
}

std::optional<server::plugins::PacketAddress> NamedPackets::resolve(std::string_view name) const {
    return packetNames::resolve(name);
}

std::optional<server::plugins::PacketScope> NamedPackets::scopeOf(std::string_view name) const {
    return packetNames::scopeOf(name);
}

std::string NamedPackets::nameOf(const server::plugins::PacketAddress& address) const {
    return packetNames::of(address);
}

std::vector<std::string> NamedPackets::names() const { return packetNames::all(); }

std::shared_ptr<server::plugins::PacketEndpoint> SessionPackets::endpointOf(server::plugins::PluginPlayerRef& player) {
    const std::string uuid = player.uuid();
    for (const player::PlayerSession* session : registry_->snapshot()) {
        if (session->uuid().toString() == uuid && session->connection().packetEndpoint) {
            return session->connection().packetEndpoint();
        }
    }
    return nullptr;
}

bool SessionPackets::send(server::plugins::PluginPlayerRef& player, const server::plugins::PacketAddress& address,
                          std::span<const std::uint8_t> bytes, bool intercept) {
    const std::shared_ptr<server::plugins::PacketEndpoint> endpoint = endpointOf(player);
    return endpoint != nullptr && endpoint->sendClientbound(address.id, bytes, intercept);
}

bool SessionPackets::receive(server::plugins::PluginPlayerRef& player, const server::plugins::PacketAddress& address,
                             std::span<const std::uint8_t> bytes, bool intercept) {
    const std::shared_ptr<server::plugins::PacketEndpoint> endpoint = endpointOf(player);
    return endpoint != nullptr && endpoint->receiveServerbound(address.id, bytes, intercept);
}

bool SessionChannels::send(server::plugins::PluginPlayerRef& player, std::string_view channel,
                           std::span<const std::uint8_t> bytes) {
    const std::string uuid = player.uuid();
    for (const player::PlayerSession* session : registry_->snapshot()) {
        if (session->uuid().toString() != uuid || !session->connection().sendPlayPacket) {
            continue;
        }
        const wire::protocol::play::CustomPayloadPacket packet{
            .channel = std::string{channel}, .data = std::vector<std::uint8_t>(bytes.begin(), bytes.end())};
        session->connection().sendPlayPacket(wire::protocol::play::CustomPayloadPacket::kClientboundId,
                                             [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    return false;
}

ServerPluginServices::ServerPluginServices(sched::Owner& owner, command::CommandDispatcher& dispatcher,
                                           std::function<std::int64_t()> currentTick,
                                           std::function<const command::CommandSource*()> console, std::string root,
                                           Parts parts)
    : tasks_(owner, std::move(currentTick)), commands_(owner, dispatcher, std::move(console)), paths_(std::move(root)),
      parts_(std::move(parts)) {}

server::plugins::PluginPlayers& ServerPluginServices::players() {
    return parts_.players == nullptr ? noPlayers() : *parts_.players;
}

server::plugins::PluginWorlds& ServerPluginServices::worlds() {
    return parts_.worlds == nullptr ? noWorlds() : *parts_.worlds;
}

server::plugins::PluginPlaceholders& ServerPluginServices::placeholders() {
    return parts_.placeholders == nullptr ? noPlaceholders() : *parts_.placeholders;
}

server::plugins::PluginStorage& ServerPluginServices::storage() {
    return parts_.storage == nullptr ? noStorage() : *parts_.storage;
}

server::plugins::PluginMenus& ServerPluginServices::menus() {
    return parts_.menus == nullptr ? noMenus() : *parts_.menus;
}

server::plugins::PluginPacketSender& ServerPluginServices::packets() {
    return parts_.packets == nullptr ? noPackets() : *parts_.packets;
}

server::plugins::PluginChannelSender& ServerPluginServices::channels() {
    return parts_.channels == nullptr ? noChannels() : *parts_.channels;
}

server::plugins::PluginInventories& ServerPluginServices::inventories() {
    return parts_.inventories == nullptr ? PluginServices::inventories() : *parts_.inventories;
}

server::plugins::PluginAccess& ServerPluginServices::access() {
    return parts_.access == nullptr ? server::plugins::PluginAccess::unavailable() : *parts_.access;
}

server::plugins::PluginScoreboards& ServerPluginServices::scoreboards() {
    return parts_.scoreboards == nullptr ? server::plugins::PluginScoreboards::unavailable() : *parts_.scoreboards;
}

server::plugins::PluginBossBars& ServerPluginServices::bossBars() {
    return parts_.bossBars == nullptr ? server::plugins::PluginBossBars::unavailable() : *parts_.bossBars;
}

server::plugins::PluginAdvancements& ServerPluginServices::advancements() {
    return parts_.advancements == nullptr ? server::plugins::PluginAdvancements::unavailable() : *parts_.advancements;
}

server::plugins::PluginServerControl& ServerPluginServices::serverControl() {
    return parts_.control == nullptr ? server::plugins::PluginServerControl::unavailable() : *parts_.control;
}

server::plugins::PluginDatapacks& ServerPluginServices::datapacks() {
    return parts_.datapacks == nullptr ? server::plugins::PluginDatapacks::unavailable() : *parts_.datapacks;
}

server::plugins::PluginMaps& ServerPluginServices::maps() {
    return parts_.maps == nullptr ? server::plugins::PluginMaps::unavailable() : *parts_.maps;
}

server::plugins::PluginLoot& ServerPluginServices::loot() {
    return parts_.loot == nullptr ? server::plugins::PluginLoot::unavailable() : *parts_.loot;
}

server::plugins::PluginStructures& ServerPluginServices::structures() {
    return parts_.structures == nullptr ? server::plugins::PluginStructures::unavailable() : *parts_.structures;
}

server::plugins::PluginSelectors& ServerPluginServices::selectors() {
    return parts_.selectors == nullptr ? server::plugins::PluginSelectors::unavailable() : *parts_.selectors;
}

bool ServerPluginServices::at(std::string_view world, double x, double z, std::function<void()> task) {
    sched::Owner* target = parts_.ownerAt ? parts_.ownerAt(std::string{world}, x, z) : nullptr;
    if (target == nullptr) {
        return false;
    }
    if (target->isCurrent()) {
        task();
    } else {
        static_cast<void>(target->submit(std::move(task)));
    }
    return true;
}

} // namespace basalt::server::runtime::plugins
