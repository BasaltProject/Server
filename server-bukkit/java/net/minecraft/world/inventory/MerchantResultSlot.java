// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.trading.Merchant;
import net.minecraft.world.item.trading.MerchantOffer;

public class MerchantResultSlot extends Slot {
    private final MerchantContainer slots;
    private final Player player;
    private final Merchant merchant;
    private int removeCount;

    public MerchantResultSlot(Player player, Merchant merchant, MerchantContainer slots, int id, int x, int y) {
        super(slots, id, x, y);
        this.player = player;
        this.merchant = merchant;
        this.slots = slots;
    }

    @Override
    public boolean mayPlace(ItemStack itemStack) {
        return false;
    }

    @Override
    public ItemStack remove(int amount) {
        if (hasItem()) {
            removeCount += Math.min(amount, getItem().getCount());
        }
        return super.remove(amount);
    }

    @Override
    protected void onQuickCraft(ItemStack picked, int count) {
        removeCount += count;
        checkTakeAchievements(picked);
    }

    @Override
    protected void checkTakeAchievements(ItemStack carried) {
        carried.onCraftedBy(player, removeCount);
        removeCount = 0;
    }

    @Override
    public boolean mayPickup(Player player) {
        return slots.getActiveOffer() != null;
    }

    @Override
    public void onTake(Player player, ItemStack carried) {
        checkTakeAchievements(carried);
        MerchantOffer offer = slots.getActiveOffer();
        if (offer != null) {
            ItemStack first = slots.getItem(0);
            ItemStack second = slots.getItem(1);
            if (offer.take(first, second) || offer.take(second, first)) {
                merchant.notifyTrade(offer);
                slots.setItem(0, first);
                slots.setItem(1, second);
            }
            merchant.overrideXp(merchant.getVillagerXp() + offer.getXp());
        }
    }
}
