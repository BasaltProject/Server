#pragma once

#include <basalt/server/runtime/world/Trades.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;

namespace villagers {
struct Offers;
}
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime {

class ClientSession;

/// Trading with a villager: the window, its two payment slots and its result.
///
/// parity: `MerchantMenu` over `MerchantContainer` and `MerchantResultSlot`.
class VillagerTradeController {
public:
    /// What a plugin gets a say in. Unset means nobody objects.
    struct Hooks {
        std::function<bool(std::int32_t containerId, std::int32_t index, const std::optional<std::string>& resultItem)>
            tradeAllowed;
    };

    static constexpr std::int32_t kPaymentA = 0;
    static constexpr std::int32_t kPaymentB = 1;
    static constexpr std::int32_t kResult = 2;

    explicit VillagerTradeController(ClientSession& owner, Hooks hooks = {});

    /// Opens a villager's trades, from the hand that clicked it.
    void openTrades(entity::ServerEntity& villager, bool mainHand = true);

    /// parity: `ClientboundMerchantOffersPacket`, sent when the window opens.
    void sendOffers();

    /// parity: `ServerboundSelectTradePacket`: `setSelectionHint` then `tryMoveItems`.
    void selectTrade(std::int32_t index);

    /// parity: `MerchantMenu.removed`: the payments go back and the villager stops trading.
    void menuClosed();

    /// parity: `AbstractVillager.stillValid`, checked on the player's own turn.
    void tick();

    [[nodiscard]] bool open() const noexcept { return openTradingWith_.has_value(); }

    /// One of the three slots.
    [[nodiscard]] const std::optional<storage::StoredItemStack>& slot(std::int32_t index) const;

    /// parity: `MerchantContainer.setItem`; emptying the result is taking it.
    void setSlot(std::int32_t index, std::optional<storage::StoredItemStack> stack);

    /// parity: `MerchantResultSlot.mayPickup`.
    [[nodiscard]] bool mayTakeResult() const;

    /// parity: `quickMoveStack` on the result, which trades again as long as it fits and pays.
    void quickMoveResult(player::PlayerSession& session);

private:
    [[nodiscard]] bool tradingStillValid();
    [[nodiscard]] std::vector<world::trades::Offer>* offers();
    /// parity: `MerchantContainer.updateSellItem`.
    void updateSellItem();
    /// parity: `MerchantResultSlot.onTake`.
    void takeResult();
    /// parity: `MerchantMenu.moveFromInventoryToPaymentSlot`.
    void moveFromInventory(player::PlayerSession& session, std::int32_t paymentSlot,
                           const server::world::loot::ItemStack& cost);

    ClientSession* owner_;
    Hooks hooks_;
    /// The villager this client is trading with, and what it offered.
    std::optional<std::int32_t> openTradingWith_;
    std::shared_ptr<entity::villagers::Offers> openOffers_;
    std::array<std::optional<storage::StoredItemStack>, 3> slots_;
    std::int32_t selectionHint_ = 0;
    std::optional<std::size_t> activeOffer_;
};

} // namespace basalt::server::runtime
