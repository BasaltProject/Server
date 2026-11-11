// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.trading.Merchant;
import net.minecraft.world.item.trading.MerchantOffer;
import net.minecraft.world.item.trading.MerchantOffers;
public class ClientSideMerchant implements Merchant {
    private final Player source;
    private MerchantOffers offers = new MerchantOffers();
    private int xp;
    public ClientSideMerchant(Player source) {
        this.source = source;
    }
    public Player getTradingPlayer() {
        return source;
    }
    public void setTradingPlayer(Player player) {
    }
    public MerchantOffers getOffers() {
        return offers;
    }
    public void overrideOffers(MerchantOffers offers) {
        this.offers = offers;
    }
    public void notifyTrade(MerchantOffer offer) {
        offer.increaseUses();
    }
    public void notifyTradeUpdated(ItemStack itemStack) {
    }
    public boolean isClientSide() {
        return source.level().isClientSide();
    }
    public boolean stillValid(Player player) {
        return source == player;
    }
    public int getVillagerXp() {
        return xp;
    }
    public void overrideXp(int xp) {
        this.xp = xp;
    }
    public boolean showProgressBar() {
        return true;
    }
    public SoundEvent getNotifyTradeSound() {
        return SoundEvents.VILLAGER_YES;


    }
}
