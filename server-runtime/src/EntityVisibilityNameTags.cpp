#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <set>

namespace basalt::server::runtime {
namespace {

using wire::protocol::play::SetPlayerTeamPacket;

[[nodiscard]] std::string nameTagTeamOf(const player::PlayerSession& player) {
    return "bnt_" + std::to_string(player.entityId());
}

[[nodiscard]] wire::common::Logger& logger() {
    static wire::common::Logger instance{"NameTags"};
    return instance;
}

} // namespace

// A client holds a member in one team only, so a vanilla team always wins over a plugin's name tag.
void EntityVisibilityTracker::syncNameTags() {
    const ClientSession::PluginHooks::Surfaces& surfaces = owner_->options().plugins.surfaces;
    player::PlayerSession* session = owner_->playerSession();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    const bool holds = surfaces.holdsNameTag && surfaces.nameTag && surfaces.holdsNameTag();
    if (session == nullptr || registry == nullptr || (!holds && sentNameTags_.empty())) {
        return;
    }
    const auto remove = [&](const std::string& name, const std::string& team) {
        owner_->sendPlayPacket(SetPlayerTeamPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetPlayerTeamPacket{.teamName = team, .method = SetPlayerTeamPacket::kMethodRemove}.write(buffer);
        });
        sentNameTags_.erase(name);
    };
    std::set<std::string> present;
    if (holds) {
        const std::shared_ptr<const std::vector<player::PlayerSession*>> everyone = registry->view();
        for (const player::PlayerSession* other : *everyone) {
            if (other->uuid() == session->uuid()) {
                continue;
            }
            const std::string team = nameTagTeamOf(*other);
            std::optional<ui::SurfaceContent> drawn;
            if (!surfaces.inVanillaTeam || !surfaces.inVanillaTeam(other->name())) {
                drawn = surfaces.nameTag(other->uuid().toString());
            }
            if (!drawn.has_value()) {
                continue;
            }
            present.insert(other->name());
            const std::string suffix = drawn->lines.empty() ? std::string{} : drawn->lines.front();
            const auto known = sentNameTags_.find(other->name());
            if (known != sentNameTags_.end() && known->second.prefix == drawn->title &&
                known->second.suffix == suffix) {
                continue;
            }
            const bool fresh = known == sentNameTags_.end();
            SetPlayerTeamPacket::Parameters parameters{.displayName = wire::text::Component::text(team),
                                                       .prefix = wire::text::miniMessage::parse(drawn->title),
                                                       .suffix = wire::text::miniMessage::parse(suffix)};
            owner_->sendPlayPacket(SetPlayerTeamPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetPlayerTeamPacket{
                    .teamName = team,
                    .method = fresh ? SetPlayerTeamPacket::kMethodAdd : SetPlayerTeamPacket::kMethodChange,
                    .parameters = parameters,
                    .members = fresh ? std::vector<std::string>{other->name()} : std::vector<std::string>{}}
                    .write(buffer);
            });
            logger().debug(
                [&] { return "Name tag team " + team + (fresh ? " added" : " changed") + " for " + other->name(); });
            sentNameTags_.insert_or_assign(other->name(), SentNameTag{team, drawn->title, suffix});
        }
    }
    std::vector<std::pair<std::string, std::string>> gone;
    for (const auto& [name, sent] : sentNameTags_) {
        if (!present.contains(name)) {
            gone.emplace_back(name, sent.team);
        }
    }
    for (const auto& [name, team] : gone) {
        remove(name, team);
    }
}

} // namespace basalt::server::runtime
