#pragma once

#include <basalt/server/runtime/world/Trades.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime {

class ClientSession;

/// Trading with a villager: the offers sent when the window opens, and the trade the player picks.
class VillagerTradeController {
public:
    /// What a plugin gets a say in. Unset means nobody objects.
    struct Hooks {
        std::function<bool(std::int32_t containerId, std::int32_t index, const std::optional<std::string>& resultItem)>
            tradeAllowed;
    };

    explicit VillagerTradeController(ClientSession& owner, Hooks hooks = {});

    /// Opens a villager's trades.
    void openTrades(entity::ServerEntity& villager);

    /// parity: `ClientboundMerchantOffersPacket`, sent when the window opens and after every trade.
    void sendOffers();

    /// Takes the trade the player picked.
    void selectTrade(std::int32_t index);

private:
    ClientSession* owner_;
    Hooks hooks_;
    /// The villager this client is trading with, and what it offered.
    std::optional<std::int32_t> openTradingWith_;
    std::vector<world::trades::Offer> openOffers_;
};

} // namespace basalt::server::runtime
