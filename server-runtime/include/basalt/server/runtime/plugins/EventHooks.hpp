#pragma once

#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/PlayerLifecycle.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>

namespace basalt::server::runtime {
class ClientSession;
}

/// The callbacks each system of one player's session calls, turned into the events plugins listen to.
namespace basalt::server::runtime::plugins::eventHooks {

[[nodiscard]] BlockInteractionHandler::Hooks blockInteraction(ClientSession& owner, const EventSink& sink);
[[nodiscard]] PlayerLifecycle::Hooks lifecycle(ClientSession& owner, const EventSink& sink);
[[nodiscard]] CombatHandler::Hooks combat(ClientSession& owner, const EventSink& sink);
[[nodiscard]] ItemUseTimers::Hooks itemUse(ClientSession& owner, const EventSink& sink);
[[nodiscard]] UiStateSync::Hooks uiState(ClientSession& owner, const EventSink& sink);
[[nodiscard]] ContainerMenuController::Hooks containerMenu(ClientSession& owner, const EventSink& sink);
[[nodiscard]] VillagerTradeController::Hooks villagerTrade(ClientSession& owner, const EventSink& sink);

} // namespace basalt::server::runtime::plugins::eventHooks
