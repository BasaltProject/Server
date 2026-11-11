// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;
import net.minecraft.network.chat.Component;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
public interface Merchant {
    void setTradingPlayer(Player player);
    Player getTradingPlayer();
    MerchantOffers getOffers();
    void overrideOffers(MerchantOffers offers);
    void notifyTrade(MerchantOffer offer);
    void notifyTradeUpdated(ItemStack itemStack);
    int getVillagerXp();
    void overrideXp(int xp);
    boolean showProgressBar();
    SoundEvent getNotifyTradeSound();
    default boolean canRestock() {
        return false;
    }
    default void openTradingScreen(Player player, Component title, int level) {
        var opened = player.openMenu(new net.minecraft.world.SimpleMenuProvider((id, inventory, ignored) -> new net.minecraft.world.inventory.MerchantMenu(id, inventory, this), title));
        if (opened.isPresent() && !getOffers().isEmpty()) {
            player.sendMerchantOffers(opened.getAsInt(), getOffers(), level, getVillagerXp(), showProgressBar(), canRestock());
        }

    }
    boolean isClientSide();
    boolean stillValid(Player player);
}
