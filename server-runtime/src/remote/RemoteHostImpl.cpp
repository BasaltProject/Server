#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/remote/RemoteHostImpl.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <memory>
#include <mutex>

namespace basalt::server::runtime::remoteHost {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

class CapturingSource final : public command::CommandSource {
public:
    CapturingSource(std::string name, wire::text::LangTable lang, command::ConsoleCommandSource::SpawnAt spawn)
        : name_(std::move(name)), lang_(std::move(lang)), spawn_(std::move(spawn)) {}

    [[nodiscard]] std::string name() const override { return name_; }

    [[nodiscard]] std::string_view kind() const override { return "console"; }

    [[nodiscard]] wire::text::Component displayName() const override { return wire::text::Component::text(name_); }

    [[nodiscard]] std::int32_t permissionLevel() const override { return command::permissionLevel::kOwners; }

    [[nodiscard]] std::array<double, 3> position() const override {
        return spawn_ ? spawn_() : std::array<double, 3>{0.0, 0.0, 0.0};
    }

    [[nodiscard]] player::PlayerSession* player() const override { return nullptr; }

    [[nodiscard]] bool hasPermission(std::optional<std::string_view>) const override { return true; }

    void sendSystemMessage(const wire::text::Component& message) const override {
        const std::lock_guard guard{lock_};
        if (!output_.empty()) {
            output_ += '\n';
        }
        output_ += wire::text::resolvePlainText(message, lang_);
    }

    [[nodiscard]] std::string take() const {
        const std::lock_guard guard{lock_};
        return std::move(output_);
    }

private:
    std::string name_;
    wire::text::LangTable lang_;
    command::ConsoleCommandSource::SpawnAt spawn_;
    mutable std::mutex lock_;
    mutable std::string output_;
};

class Host final : public server::remote::RemoteHost {
public:
    explicit Host(Sources sources) : s_(sources) {}

    void runOnServerThread(std::function<void()> work) override {
        const command::CommandEnvironment& environment = s_.dispatcher->environment();
        const auto named = environment.options().worlds.find(environment.options().defaultWorldName);
        if (named == environment.options().worlds.end() || named->second == nullptr) {
            work();
            return;
        }
        static_cast<void>(named->second->owner().submitConsole(std::move(work)));
    }

    [[nodiscard]] server::remote::CommandResult executeCommand(server::remote::Origin origin, std::string_view peer,
                                                               std::string_view text) override {
        const command::CommandEnvironment& environment = s_.dispatcher->environment();
        const auto named = environment.options().worlds.find(environment.options().defaultWorldName);
        world::LoadedWorld* defaultWorld = named == environment.options().worlds.end() ? nullptr : named->second;
        CapturingSource source{origin == server::remote::Origin::Rcon ? "Rcon" : "Api", environment.lang(),
                               [defaultWorld]() -> std::array<double, 3> {
                                   return defaultWorld == nullptr ? std::array<double, 3>{0.0, 0.0, 0.0}
                                                                  : defaultWorld->spawn();
                               }};
        std::string line{text};
        if (line.starts_with('/')) {
            line.erase(0, 1);
        }
        server::plugins::PluginEventBus* bus = environment.pluginEvents();
        const std::string sender = origin == server::remote::Origin::Rcon ? "rcon" : "api";
        const server::plugins::PluginEvent* asked = server::plugins::emit(
            bus, bus == nullptr ? nullptr : &server::plugins::pluginEventTypes::serverRemoteCommand(),
            [&](server::plugins::PluginEvent& event) {
                event.set(0, sender);
                event.set(1, std::string{peer});
                event.set(2, line);
            });
        if (asked != nullptr && asked->cancelled()) {
            return {true, asked->cancelMessage().value_or("")};
        }
        if (asked != nullptr) {
            if (const auto* edited = std::get_if<std::string>(&asked->get("command")); edited != nullptr) {
                line = *edited;
            }
        }
        static_cast<void>(s_.dispatcher->execute(source, line));
        return {false, source.take()};
    }

    [[nodiscard]] std::string statusJson() override {
        JsonObject object;
        object.set("version", JsonValue::of(std::string{buildInfo::kVersion}));
        object.set("minecraft", JsonValue::of(std::string{buildInfo::kMinecraftVersion}));
        object.set("protocol", JsonValue::of(static_cast<std::int64_t>(buildInfo::kProtocolVersion)));
        const TickRates rates = s_.tick->tickRates();
        object.set("tps", JsonValue::of(rates.oneMinute));
        double total = 0.0;
        const std::vector<std::int64_t> recent = s_.tick->recentTickNanos();
        for (const std::int64_t nanos : recent) {
            total += static_cast<double>(nanos);
        }
        object.set("mspt", JsonValue::of(recent.empty() ? 0.0 : total / static_cast<double>(recent.size()) / 1.0e6));
        const auto uptime = std::chrono::steady_clock::now() - s_.startedAt;
        object.set("uptime_seconds", JsonValue::of(static_cast<std::int64_t>(
                                         std::chrono::duration_cast<std::chrono::seconds>(uptime).count())));
        JsonObject players;
        players.set("online", JsonValue::of(static_cast<std::int64_t>(s_.players->count())));
        players.set("max", JsonValue::of(static_cast<std::int64_t>(s_.maxPlayers)));
        object.set("players", JsonValue::ofObject(std::move(players)));
        return wire::common::writeJson(JsonValue::ofObject(std::move(object)));
    }

    [[nodiscard]] std::string playersJson() override {
        std::vector<JsonValue> list;
        for (player::PlayerSession* session : s_.players->snapshot()) {
            JsonObject entry;
            entry.set("name", JsonValue::of(session->name()));
            entry.set("uuid", JsonValue::of(session->uuid().toString()));
            entry.set("world", JsonValue::of(session->worldName));
            entry.set("x", JsonValue::of(session->x));
            entry.set("y", JsonValue::of(session->y));
            entry.set("z", JsonValue::of(session->z));
            list.push_back(JsonValue::ofObject(std::move(entry)));
        }
        JsonObject object;
        object.set("players", JsonValue::ofArray(std::move(list)));
        return wire::common::writeJson(JsonValue::ofObject(std::move(object)));
    }

    [[nodiscard]] std::string pluginsJson() override {
        std::vector<JsonValue> list;
        for (const server::plugins::PluginStatus& status : s_.plugins->statuses()) {
            JsonObject entry;
            entry.set("id", JsonValue::of(status.id));
            entry.set("name", JsonValue::of(status.name));
            entry.set("version", JsonValue::of(status.version));
            entry.set("origin", JsonValue::of(status.origin));
            entry.set("disabled", JsonValue::of(status.disabled));
            entry.set("errors", JsonValue::of(static_cast<std::int64_t>(status.errors)));
            std::vector<JsonValue> commands;
            for (const std::string& name : status.commands) {
                commands.push_back(JsonValue::of(name));
            }
            entry.set("commands", JsonValue::ofArray(std::move(commands)));
            list.push_back(JsonValue::ofObject(std::move(entry)));
        }
        JsonObject object;
        object.set("plugins", JsonValue::ofArray(std::move(list)));
        return wire::common::writeJson(JsonValue::ofObject(std::move(object)));
    }

private:
    Sources s_;
};

} // namespace

std::unique_ptr<server::remote::RemoteHost> create(Sources sources) { return std::make_unique<Host>(sources); }

} // namespace basalt::server::runtime::remoteHost
