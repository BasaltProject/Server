#pragma once

#include <basalt/wire/protocol/play/PlayPackets.hpp>

namespace basalt::server::runtime::player {

class PlayerRegistry;
class PlayerSession;

[[nodiscard]] bool hasCustomListEntry(const PlayerSession& subject);

void sendListEntry(const PlayerSession& viewer, const PlayerSession& subject, bool listedForViewer = true);

void announceListEntry(const PlayerRegistry& registry, const PlayerSession& subject);

void sendTabHeaderFooter(const PlayerSession& viewer);

} // namespace basalt::server::runtime::player
