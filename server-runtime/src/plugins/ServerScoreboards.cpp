#include <basalt/server/runtime/plugins/ServerScoreboards.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins {
namespace {

namespace play = wire::protocol::play;

using scoreboard::Objective;
using scoreboard::Team;
using server::plugins::PluginObjective;
using server::plugins::PluginScoreDisplay;
using server::plugins::PluginTeam;
using wire::text::Component;

constexpr std::int32_t kSlotCount = 19;

[[nodiscard]] Component fromJson(const std::string& text) {
    if (text.starts_with("mini:")) {
        return wire::text::miniMessage::parse(std::string_view{text}.substr(5));
    }
    const auto parsed = wire::common::parseJson(text);
    return parsed.has_value() ? wire::text::componentJson::deserialize(*parsed) : Component::text(text);
}

[[nodiscard]] std::string toJson(const Component& component) {
    return wire::common::writeJson(wire::text::componentJson::serialize(component));
}

[[nodiscard]] std::optional<play::NumberFormat> formatOf(const std::string& kind, const std::string& text) {
    if (kind == "blank") {
        return play::NumberFormat{play::NumberFormat::Blank{}};
    }
    if (kind == "fixed") {
        return play::NumberFormat{play::NumberFormat::Fixed{fromJson(text)}};
    }
    if (kind == "styled") {
        wire::nbt::NbtCompound style;
        const wire::nbt::NbtTag packed = wire::text::componentNbt::serialize(fromJson(text));
        if (const auto* compound = packed.get<wire::nbt::NbtCompound>(); compound != nullptr) {
            style = *compound;
            style.remove("text");
        }
        return play::NumberFormat{play::NumberFormat::Styled{std::move(style)}};
    }
    return std::nullopt;
}

void describe(const std::optional<play::NumberFormat>& format, std::string& kind, std::string& text) {
    kind.clear();
    text.clear();
    if (!format.has_value()) {
        return;
    }
    if (format->as<play::NumberFormat::Blank>() != nullptr) {
        kind = "blank";
    } else if (const auto* fixed = format->as<play::NumberFormat::Fixed>(); fixed != nullptr) {
        kind = "fixed";
        text = toJson(fixed->value);
    } else if (const auto* styled = format->as<play::NumberFormat::Styled>(); styled != nullptr) {
        kind = "styled";
        wire::nbt::NbtCompound carrier = styled->style;
        carrier.putString("text", "");
        text = toJson(wire::text::componentNbt::deserialize(wire::nbt::NbtTag::ofCompound(std::move(carrier))));
    }
}

[[nodiscard]] PluginObjective pack(const Objective& objective) {
    PluginObjective out;
    out.name = objective.name;
    out.criterion = objective.criterion;
    out.displayName = toJson(objective.displayName);
    out.hearts = objective.renderType == play::ObjectiveRenderType::Hearts;
    out.autoUpdate = objective.displayAutoUpdate;
    out.readOnly = Objective::isReadOnly(objective.criterion);
    describe(objective.numberFormat, out.numberFormat, out.numberFormatText);
    return out;
}

[[nodiscard]] PluginTeam pack(const Team& team) {
    PluginTeam out;
    out.name = team.name;
    out.displayName = toJson(team.displayName);
    out.prefix = toJson(team.prefix);
    out.suffix = toJson(team.suffix);
    out.color = team.color.has_value() ? std::string{Objective::teamColorName(*team.color)} : std::string{};
    out.friendlyFire = team.friendlyFire;
    out.seeFriendlyInvisibles = team.seeFriendlyInvisibles;
    out.nameTagVisibility = std::string{play::nameTagVisibilityName(team.nameTagVisibility)};
    out.deathMessageVisibility = std::string{play::nameTagVisibilityName(team.deathMessageVisibility)};
    out.collisionRule = std::string{play::collisionRuleName(team.collisionRule)};
    out.members = team.members;
    return out;
}

void apply(const PluginTeam& from, Team& team) {
    team.displayName = fromJson(from.displayName);
    team.prefix = fromJson(from.prefix);
    team.suffix = fromJson(from.suffix);
    team.color = from.color.empty() ? std::nullopt : play::teamColorByName(from.color);
    team.friendlyFire = from.friendlyFire;
    team.seeFriendlyInvisibles = from.seeFriendlyInvisibles;
    team.nameTagVisibility = play::nameTagVisibilityByName(from.nameTagVisibility).value_or(team.nameTagVisibility);
    team.deathMessageVisibility =
        play::nameTagVisibilityByName(from.deathMessageVisibility).value_or(team.deathMessageVisibility);
    team.collisionRule = play::collisionRuleByName(from.collisionRule).value_or(team.collisionRule);
}

void sendPacket(const player::PlayerSession& player, std::int32_t id,
                const std::function<void(wire::io::ByteBuffer&)>& write) {
    if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(id, write);
    }
}

[[nodiscard]] play::SetObjectivePacket objectivePacket(const Objective& objective, std::uint8_t method) {
    play::SetObjectivePacket packet;
    packet.objectiveName = objective.name;
    packet.method = method;
    if (method != play::SetObjectivePacket::kMethodRemove) {
        packet.displayName = objective.displayName;
        packet.renderType = objective.renderType;
        packet.numberFormat = objective.numberFormat;
    }
    return packet;
}

[[nodiscard]] play::SetScorePacket scorePacket(const scoreboard::Scoreboard& board, std::string_view entry,
                                               std::string_view objective) {
    play::SetScorePacket packet;
    packet.owner = std::string{entry};
    packet.objectiveName = std::string{objective};
    packet.score = board.score(entry, objective).value_or(0);
    if (const Component* custom = board.displayNameOf(entry, objective); custom != nullptr) {
        packet.display = *custom;
    }
    if (const play::NumberFormat* format = board.numberFormatOf(entry, objective); format != nullptr) {
        packet.numberFormat = *format;
    }
    return packet;
}

[[nodiscard]] play::SetPlayerTeamPacket teamPacket(const Team& team, std::uint8_t method) {
    play::SetPlayerTeamPacket packet;
    packet.teamName = team.name;
    packet.method = method;
    if (method == play::SetPlayerTeamPacket::kMethodAdd || method == play::SetPlayerTeamPacket::kMethodChange) {
        packet.parameters = team.parameters();
    }
    if (method == play::SetPlayerTeamPacket::kMethodAdd) {
        packet.members = team.members;
    }
    return packet;
}

} // namespace

ServerScoreboards::ServerScoreboards(Hooks hooks) : hooks_(std::move(hooks)) {
    Board main;
    main.scores = hooks_.main;
    main.teams = hooks_.mainTeams;
    boards_.emplace(kMain, std::move(main));
}

ServerScoreboards::Board* ServerScoreboards::find(std::int32_t board) {
    const auto found = boards_.find(board);
    return found == boards_.end() || found->second.scores == nullptr ? nullptr : &found->second;
}

std::vector<player::PlayerSession*> ServerScoreboards::viewersOf(std::int32_t board) const {
    std::vector<player::PlayerSession*> viewers;
    if (!hooks_.sessions) {
        return viewers;
    }
    for (player::PlayerSession* session : hooks_.sessions()) {
        const auto assigned = viewing_.find(session->uuid().toString());
        const std::int32_t shown = assigned == viewing_.end() ? kMain : assigned->second;
        if (shown == board) {
            viewers.push_back(session);
        }
    }
    return viewers;
}

void ServerScoreboards::send(std::int32_t board, const std::function<void(const player::PlayerSession&)>& one) const {
    for (player::PlayerSession* session : viewersOf(board)) {
        one(*session);
    }
}

void ServerScoreboards::sendObjective(std::int32_t board, const Objective& objective, std::uint8_t method) const {
    const play::SetObjectivePacket packet = objectivePacket(objective, method);
    send(board, [&packet](const player::PlayerSession& player) {
        sendPacket(player, play::SetObjectivePacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    });
}

void ServerScoreboards::sendScore(std::int32_t board, std::string_view entry, std::string_view objective) const {
    const auto found = boards_.find(board);
    if (found == boards_.end()) {
        return;
    }
    const play::SetScorePacket packet = scorePacket(*found->second.scores, entry, objective);
    send(board, [&packet](const player::PlayerSession& player) {
        sendPacket(player, play::SetScorePacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    });
}

void ServerScoreboards::sendTeam(std::int32_t board, const Team& team, std::uint8_t method) const {
    const play::SetPlayerTeamPacket packet = teamPacket(team, method);
    send(board, [&packet](const player::PlayerSession& player) {
        sendPacket(player, play::SetPlayerTeamPacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    });
}

void ServerScoreboards::drawAll(const player::PlayerSession& player, Board& board) const {
    for (const Objective* objective : std::as_const(*board.scores).allObjectives()) {
        const play::SetObjectivePacket packet = objectivePacket(*objective, play::SetObjectivePacket::kMethodAdd);
        sendPacket(player, play::SetObjectivePacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        for (const std::string& entry : board.scores->holdersIn(objective->name)) {
            const play::SetScorePacket score = scorePacket(*board.scores, entry, objective->name);
            sendPacket(player, play::SetScorePacket::kId,
                       [&score](wire::io::ByteBuffer& buffer) { score.write(buffer); });
        }
    }
    for (std::int32_t slot = 0; slot < kSlotCount; ++slot) {
        if (const std::string* shown = board.scores->displayed(static_cast<play::DisplaySlot>(slot));
            shown != nullptr) {
            const play::SetDisplayObjectivePacket packet{.slot = static_cast<play::DisplaySlot>(slot),
                                                         .objectiveName = *shown};
            sendPacket(player, play::SetDisplayObjectivePacket::kId,
                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    for (const Team* team : std::as_const(*board.teams).all()) {
        const play::SetPlayerTeamPacket packet = teamPacket(*team, play::SetPlayerTeamPacket::kMethodAdd);
        sendPacket(player, play::SetPlayerTeamPacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

void ServerScoreboards::eraseAll(const player::PlayerSession& player, Board& board) const {
    for (const Team* team : std::as_const(*board.teams).all()) {
        const play::SetPlayerTeamPacket packet = teamPacket(*team, play::SetPlayerTeamPacket::kMethodRemove);
        sendPacket(player, play::SetPlayerTeamPacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
    for (const Objective* objective : std::as_const(*board.scores).allObjectives()) {
        const play::SetObjectivePacket packet = objectivePacket(*objective, play::SetObjectivePacket::kMethodRemove);
        sendPacket(player, play::SetObjectivePacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

void ServerScoreboards::joined(const player::PlayerSession& player) {
    viewing_.erase(player.uuid().toString());
    if (Board* main = find(kMain); main != nullptr) {
        drawAll(player, *main);
    }
}

void ServerScoreboards::left(const std::string& uuid) { viewing_.erase(uuid); }

std::int32_t ServerScoreboards::create() {
    Board board;
    board.ownedScores = std::make_unique<scoreboard::Scoreboard>();
    board.ownedTeams = std::make_unique<scoreboard::Teams>();
    board.scores = board.ownedScores.get();
    board.teams = board.ownedTeams.get();
    const std::int32_t handle = next_++;
    boards_.emplace(handle, std::move(board));
    return handle;
}

void ServerScoreboards::release(std::int32_t board) {
    if (board == kMain || find(board) == nullptr) {
        return;
    }
    std::vector<std::string> shown;
    for (const auto& [uuid, assigned] : viewing_) {
        if (assigned == board) {
            shown.push_back(uuid);
        }
    }
    for (const std::string& uuid : shown) {
        show(uuid, kMain);
    }
    boards_.erase(board);
}

std::vector<PluginObjective> ServerScoreboards::objectives(std::int32_t board) {
    std::vector<PluginObjective> out;
    if (Board* one = find(board); one != nullptr) {
        for (const Objective* objective : std::as_const(*one->scores).allObjectives()) {
            out.push_back(pack(*objective));
        }
    }
    return out;
}

std::optional<PluginObjective> ServerScoreboards::objective(std::int32_t board, std::string_view name) {
    Board* one = find(board);
    const Objective* found = one == nullptr ? nullptr : std::as_const(*one->scores).get(name);
    return found == nullptr ? std::nullopt : std::optional{pack(*found)};
}

bool ServerScoreboards::addObjective(std::int32_t board, const PluginObjective& made) {
    Board* one = find(board);
    const std::optional<std::string> criterion = Objective::canonical(made.criterion);
    if (one == nullptr || !criterion.has_value()) {
        return false;
    }
    Objective objective;
    objective.name = made.name;
    objective.criterion = *criterion;
    objective.displayName = fromJson(made.displayName);
    objective.renderType = made.hearts ? play::ObjectiveRenderType::Hearts : play::ObjectiveRenderType::Integer;
    objective.numberFormat = formatOf(made.numberFormat, made.numberFormatText);
    objective.displayAutoUpdate = made.autoUpdate;
    if (!one->scores->add(objective)) {
        return false;
    }
    sendObjective(board, objective, play::SetObjectivePacket::kMethodAdd);
    return true;
}

bool ServerScoreboards::updateObjective(std::int32_t board, const PluginObjective& changed) {
    Board* one = find(board);
    Objective* objective = one == nullptr ? nullptr : one->scores->get(changed.name);
    if (objective == nullptr) {
        return false;
    }
    objective->displayName = fromJson(changed.displayName);
    objective->renderType = changed.hearts ? play::ObjectiveRenderType::Hearts : play::ObjectiveRenderType::Integer;
    objective->numberFormat = formatOf(changed.numberFormat, changed.numberFormatText);
    objective->displayAutoUpdate = changed.autoUpdate;
    sendObjective(board, *objective, play::SetObjectivePacket::kMethodChange);
    return true;
}

bool ServerScoreboards::removeObjective(std::int32_t board, std::string_view name) {
    Board* one = find(board);
    if (one == nullptr) {
        return false;
    }
    std::unique_ptr<Objective> removed = one->scores->remove(name);
    if (removed == nullptr) {
        return false;
    }
    sendObjective(board, *removed, play::SetObjectivePacket::kMethodRemove);
    return true;
}

std::optional<std::string> ServerScoreboards::displayed(std::int32_t board, std::int32_t slot) {
    Board* one = find(board);
    const std::string* shown = one == nullptr ? nullptr : one->scores->displayed(static_cast<play::DisplaySlot>(slot));
    return shown == nullptr ? std::nullopt : std::optional{*shown};
}

void ServerScoreboards::display(std::int32_t board, std::int32_t slot, const std::optional<std::string>& objective) {
    Board* one = find(board);
    if (one == nullptr || slot < 0 || slot >= kSlotCount ||
        (objective.has_value() && one->scores->get(*objective) == nullptr)) {
        return;
    }
    one->scores->display(static_cast<play::DisplaySlot>(slot), objective);
    const play::SetDisplayObjectivePacket packet{.slot = static_cast<play::DisplaySlot>(slot),
                                                 .objectiveName = objective.value_or(std::string{})};
    send(board, [&packet](const player::PlayerSession& player) {
        sendPacket(player, play::SetDisplayObjectivePacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    });
}

std::optional<std::int32_t> ServerScoreboards::score(std::int32_t board, std::string_view entry,
                                                     std::string_view objective) {
    Board* one = find(board);
    return one == nullptr ? std::nullopt : std::as_const(*one->scores).score(entry, objective);
}

bool ServerScoreboards::setScore(std::int32_t board, std::string_view entry, std::string_view objective,
                                 std::int32_t value) {
    Board* one = find(board);
    if (one == nullptr || one->scores->get(objective) == nullptr) {
        return false;
    }
    one->scores->setScore(entry, objective, value);
    sendScore(board, entry, objective);
    return true;
}

void ServerScoreboards::resetScore(std::int32_t board, std::string_view entry,
                                   const std::optional<std::string>& objective) {
    Board* one = find(board);
    if (one == nullptr) {
        return;
    }
    one->scores->resetScore(entry, objective.has_value() ? std::optional<std::string_view>{*objective} : std::nullopt);
    const play::ResetScorePacket packet{.owner = std::string{entry}, .objectiveName = objective};
    send(board, [&packet](const player::PlayerSession& player) {
        sendPacket(player, play::ResetScorePacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    });
}

std::vector<std::string> ServerScoreboards::entries(std::int32_t board) {
    Board* one = find(board);
    return one == nullptr ? std::vector<std::string>{} : std::as_const(*one->scores).holders();
}

std::vector<std::string> ServerScoreboards::entriesIn(std::int32_t board, std::string_view objective) {
    Board* one = find(board);
    return one == nullptr ? std::vector<std::string>{} : std::as_const(*one->scores).holdersIn(objective);
}

std::vector<std::pair<std::string, std::int32_t>> ServerScoreboards::scoresOf(std::int32_t board,
                                                                              std::string_view entry) {
    Board* one = find(board);
    return one == nullptr ? std::vector<std::pair<std::string, std::int32_t>>{}
                          : std::as_const(*one->scores).scoresOf(entry);
}

PluginScoreDisplay ServerScoreboards::scoreDisplay(std::int32_t board, std::string_view entry,
                                                   std::string_view objective) {
    PluginScoreDisplay out;
    Board* one = find(board);
    if (one == nullptr) {
        return out;
    }
    if (const Component* custom = std::as_const(*one->scores).displayNameOf(entry, objective); custom != nullptr) {
        out.customName = toJson(*custom);
    }
    if (const play::NumberFormat* format = std::as_const(*one->scores).numberFormatOf(entry, objective);
        format != nullptr) {
        describe(*format, out.numberFormat, out.numberFormatText);
    }
    return out;
}

void ServerScoreboards::setScoreDisplay(std::int32_t board, std::string_view entry, std::string_view objective,
                                        const PluginScoreDisplay& display) {
    Board* one = find(board);
    if (one == nullptr || one->scores->get(objective) == nullptr) {
        return;
    }
    one->scores->setDisplayName(
        entry, objective, display.customName.has_value() ? std::optional{fromJson(*display.customName)} : std::nullopt);
    one->scores->setNumberFormat(entry, objective, formatOf(display.numberFormat, display.numberFormatText));
    sendScore(board, entry, objective);
}

bool ServerScoreboards::triggerable(std::int32_t board, std::string_view entry, std::string_view objective) {
    Board* one = find(board);
    return one != nullptr && std::as_const(*one->scores).isTriggerEnabled(entry, objective);
}

void ServerScoreboards::setTriggerable(std::int32_t board, std::string_view entry, std::string_view objective,
                                       bool value) {
    Board* one = find(board);
    if (one == nullptr || one->scores->get(objective) == nullptr) {
        return;
    }
    if (value) {
        one->scores->enableTrigger(entry, objective);
    } else {
        one->scores->disableTrigger(entry, objective);
    }
}

std::vector<PluginTeam> ServerScoreboards::teams(std::int32_t board) {
    std::vector<PluginTeam> out;
    if (Board* one = find(board); one != nullptr) {
        for (const Team* team : std::as_const(*one->teams).all()) {
            out.push_back(pack(*team));
        }
    }
    return out;
}

std::optional<PluginTeam> ServerScoreboards::team(std::int32_t board, std::string_view name) {
    Board* one = find(board);
    const Team* found = one == nullptr ? nullptr : std::as_const(*one->teams).get(name);
    return found == nullptr ? std::nullopt : std::optional{pack(*found)};
}

std::optional<PluginTeam> ServerScoreboards::teamOf(std::int32_t board, std::string_view entry) {
    Board* one = find(board);
    const Team* found = one == nullptr ? nullptr : std::as_const(*one->teams).teamOf(entry);
    return found == nullptr ? std::nullopt : std::optional{pack(*found)};
}

bool ServerScoreboards::addTeam(std::int32_t board, std::string_view name) {
    Board* one = find(board);
    Team* made = one == nullptr ? nullptr : one->teams->add(std::string{name});
    if (made == nullptr) {
        return false;
    }
    sendTeam(board, *made, play::SetPlayerTeamPacket::kMethodAdd);
    return true;
}

bool ServerScoreboards::updateTeam(std::int32_t board, const PluginTeam& changed) {
    Board* one = find(board);
    Team* team = one == nullptr ? nullptr : one->teams->get(changed.name);
    if (team == nullptr) {
        return false;
    }
    apply(changed, *team);
    sendTeam(board, *team, play::SetPlayerTeamPacket::kMethodChange);
    return true;
}

bool ServerScoreboards::removeTeam(std::int32_t board, std::string_view name) {
    Board* one = find(board);
    std::unique_ptr<Team> removed = one == nullptr ? nullptr : one->teams->remove(name);
    if (removed == nullptr) {
        return false;
    }
    sendTeam(board, *removed, play::SetPlayerTeamPacket::kMethodRemove);
    return true;
}

bool ServerScoreboards::addMember(std::int32_t board, std::string_view teamName, std::string_view entry) {
    Board* one = find(board);
    Team* team = one == nullptr ? nullptr : one->teams->get(teamName);
    if (team == nullptr) {
        return false;
    }
    if (team->hasMember(entry)) {
        return false;
    }
    if (Team* previous = one->teams->join(*team, std::string{entry}); previous != nullptr) {
        const play::SetPlayerTeamPacket left{.teamName = previous->name,
                                             .method = play::SetPlayerTeamPacket::kMethodLeave,
                                             .members = {std::string{entry}}};
        send(board, [&left](const player::PlayerSession& player) {
            sendPacket(player, play::SetPlayerTeamPacket::kId,
                       [&left](wire::io::ByteBuffer& buffer) { left.write(buffer); });
        });
    }
    const play::SetPlayerTeamPacket joined{
        .teamName = team->name, .method = play::SetPlayerTeamPacket::kMethodJoin, .members = {std::string{entry}}};
    send(board, [&joined](const player::PlayerSession& player) {
        sendPacket(player, play::SetPlayerTeamPacket::kId,
                   [&joined](wire::io::ByteBuffer& buffer) { joined.write(buffer); });
    });
    return true;
}

bool ServerScoreboards::removeMember(std::int32_t board, std::string_view teamName, std::string_view entry) {
    Board* one = find(board);
    Team* team = one == nullptr ? nullptr : one->teams->get(teamName);
    if (team == nullptr || !team->removeMember(entry)) {
        return false;
    }
    const play::SetPlayerTeamPacket left{
        .teamName = team->name, .method = play::SetPlayerTeamPacket::kMethodLeave, .members = {std::string{entry}}};
    send(board, [&left](const player::PlayerSession& player) {
        sendPacket(player, play::SetPlayerTeamPacket::kId,
                   [&left](wire::io::ByteBuffer& buffer) { left.write(buffer); });
    });
    return true;
}

void ServerScoreboards::show(std::string_view uuid, std::int32_t board) {
    Board* target = find(board);
    if (target == nullptr || !hooks_.sessions) {
        return;
    }
    const auto assigned = viewing_.find(uuid);
    const std::int32_t before = assigned == viewing_.end() ? kMain : assigned->second;
    if (before == board) {
        return;
    }
    for (player::PlayerSession* session : hooks_.sessions()) {
        if (session->uuid().toString() != uuid) {
            continue;
        }
        if (Board* old = find(before); old != nullptr) {
            eraseAll(*session, *old);
        }
        drawAll(*session, *target);
    }
    if (board == kMain) {
        viewing_.erase(std::string{uuid});
    } else {
        viewing_.insert_or_assign(std::string{uuid}, board);
    }
}

std::int32_t ServerScoreboards::shown(std::string_view uuid) {
    const auto assigned = viewing_.find(uuid);
    return assigned == viewing_.end() ? kMain : assigned->second;
}

} // namespace basalt::server::runtime::plugins
