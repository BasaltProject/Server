// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.core.NonNullList;
import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.trading.Merchant;
import net.minecraft.world.item.trading.MerchantOffer;
import net.minecraft.world.item.trading.MerchantOffers;

public class MerchantContainer implements Container {
    private final Merchant merchant;
    private final NonNullList<ItemStack> itemStacks = NonNullList.withSize(3, ItemStack.EMPTY);
    private MerchantOffer activeOffer;
    private int selectionHint;
    private int futureXp;

    public MerchantContainer(Merchant villager) {
        this.merchant = villager;
    }

    @Override
    public int getContainerSize() {
        return itemStacks.size();
    }

    @Override
    public boolean isEmpty() {
        for (ItemStack stack : itemStacks) {
            if (!stack.isEmpty()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public ItemStack getItem(int slot) {
        return itemStacks.get(slot);
    }

    @Override
    public ItemStack removeItem(int slot, int count) {
        ItemStack stack = itemStacks.get(slot);
        if (slot == 2 && !stack.isEmpty()) {
            return splitFrom(slot, stack.getCount());
        }
        ItemStack removed = splitFrom(slot, count);
        if (!removed.isEmpty() && isPaymentSlot(slot)) {
            updateSellItem();
        }
        return removed;
    }

    private ItemStack splitFrom(int slot, int count) {
        ItemStack stack = itemStacks.get(slot);
        return count > 0 && !stack.isEmpty() ? stack.split(count) : ItemStack.EMPTY;
    }

    private boolean isPaymentSlot(int slot) {
        return slot == 0 || slot == 1;
    }

    @Override
    public ItemStack removeItemNoUpdate(int slot) {
        ItemStack stack = itemStacks.get(slot);
        itemStacks.set(slot, ItemStack.EMPTY);
        return stack.isEmpty() ? ItemStack.EMPTY : stack;
    }

    @Override
    public void setItem(int slot, ItemStack itemStack) {
        itemStacks.set(slot, itemStack);
        itemStack.limitSize(getMaxStackSize(itemStack));
        if (isPaymentSlot(slot)) {
            updateSellItem();
        }
    }

    @Override
    public boolean stillValid(Player player) {
        return merchant == null || merchant.getTradingPlayer() == player;
    }

    @Override
    public void setChanged() {
        updateSellItem();
    }

    public void updateSellItem() {
        activeOffer = null;
        ItemStack first = itemStacks.get(0);
        ItemStack second = itemStacks.get(1);
        if (first.isEmpty()) {
            first = second;
            second = ItemStack.EMPTY;
        }
        if (first.isEmpty()) {
            itemStacks.set(2, ItemStack.EMPTY);
            futureXp = 0;
            return;
        }
        MerchantOffers offers = merchant == null ? new MerchantOffers() : merchant.getOffers();
        if (!offers.isEmpty()) {
            MerchantOffer offer = offers.getRecipeFor(first, second, selectionHint);
            if (offer == null || offer.isOutOfStock()) {
                offer = offers.getRecipeFor(second, first, selectionHint);
            }
            if (offer != null && !offer.isOutOfStock()) {
                activeOffer = offer;
                itemStacks.set(2, offer.assemble());
                futureXp = offer.getXp();
            } else {
                itemStacks.set(2, ItemStack.EMPTY);
                futureXp = 0;
            }
        }
        if (merchant != null) {
            merchant.notifyTradeUpdated(itemStacks.get(2));
        }
    }

    public MerchantOffer getActiveOffer() {
        return activeOffer;
    }

    public void setSelectionHint(int selectionHint) {
        this.selectionHint = selectionHint;
        updateSellItem();
    }

    @Override
    public void clearContent() {
        itemStacks.clear();
        for (int i = 0; i < 3; i++) {
            itemStacks.add(ItemStack.EMPTY);
        }
    }

    public int getFutureXp() {
        return futureXp;
    }
}
