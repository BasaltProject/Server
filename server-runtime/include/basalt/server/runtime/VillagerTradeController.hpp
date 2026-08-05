#pragma once

#include <basalt/server/runtime/world/Trades.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/text/Component.hpp>

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

class VillagerTradeController {
public:
    struct Hooks {
        std::function<bool(std::int32_t containerId, std::int32_t index, const std::optional<std::string>& resultItem)>
            tradeAllowed;
    };

    static constexpr std::int32_t kPaymentA = 0;
    static constexpr std::int32_t kPaymentB = 1;
    static constexpr std::int32_t kResult = 2;

    explicit VillagerTradeController(ClientSession& owner, Hooks hooks = {});

    void openTrades(entity::ServerEntity& villager, bool mainHand = true);

    /// Opens the merchant window on a list of trades a plugin made, with no villager behind it.
    bool openCustom(const wire::text::Component& title, std::vector<world::trades::Offer> offers);

    void sendOffers();

    void selectTrade(std::int32_t index);

    void menuClosed();

    void tick();

    [[nodiscard]] bool open() const noexcept { return openTradingWith_.has_value() || custom_; }

    [[nodiscard]] std::optional<std::int32_t> tradingWith() const noexcept { return openTradingWith_; }

    [[nodiscard]] const std::optional<storage::StoredItemStack>& slot(std::int32_t index) const;

    void setSlot(std::int32_t index, std::optional<storage::StoredItemStack> stack);

    [[nodiscard]] bool mayTakeResult() const;

    void quickMoveResult(player::PlayerSession& session);

private:
    [[nodiscard]] bool tradingStillValid();
    void sendOfferList(const entity::ServerEntity* villager);
    [[nodiscard]] std::vector<world::trades::Offer>* offers();
    void updateSellItem();
    void takeResult();
    void moveFromInventory(player::PlayerSession& session, std::int32_t paymentSlot,
                           const server::world::loot::ItemStack& cost);

    ClientSession* owner_;
    Hooks hooks_;
    std::optional<std::int32_t> openTradingWith_;
    bool custom_ = false;
    std::shared_ptr<entity::villagers::Offers> openOffers_;
    std::array<std::optional<storage::StoredItemStack>, 3> slots_;
    std::int32_t selectionHint_ = 0;
    std::optional<std::size_t> activeOffer_;
};

} // namespace basalt::server::runtime
