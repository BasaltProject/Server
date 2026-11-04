// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.trading.Merchant;
import net.minecraft.world.item.trading.MerchantOffers;

public class MerchantMenu extends AbstractContainerMenu {
    protected static final int PAYMENT1_SLOT = 0;
    protected static final int PAYMENT2_SLOT = 1;
    protected static final int RESULT_SLOT = 2;
    private static final int kInventoryStart = 3;
    private static final int kInventoryEnd = 30;
    private static final int kUseRowStart = 30;
    private static final int kUseRowEnd = 39;
    private final MerchantContainer tradeContainer;
    private final Merchant trader;
    private MerchantOffers offers = new MerchantOffers();
    private int merchantLevel;
    private int traderXp;
    private boolean showProgressBar;
    private boolean canRestock;

    public MerchantMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, null);
    }

    public MerchantMenu(int containerId, Inventory inventory, Merchant merchant) {
        super(MenuType.MERCHANT, containerId);
        this.trader = merchant;
        this.tradeContainer = new MerchantContainer(merchant);
        addSlot(new Slot(tradeContainer, PAYMENT1_SLOT, 136, 37));
        addSlot(new Slot(tradeContainer, PAYMENT2_SLOT, 162, 37));
        addSlot(new MerchantResultSlot(inventory.player, merchant, tradeContainer, RESULT_SLOT, 220, 37));
        addStandardInventorySlots(inventory, 108, 84);
    }

    public void setShowProgressBar(boolean show) {
        this.showProgressBar = show;
    }

    public boolean showProgressBar() {
        return showProgressBar;
    }

    public void setSelectionHint(int hint) {
        tradeContainer.setSelectionHint(hint);
    }

    public void slotsChanged(Container container) {
        tradeContainer.updateSellItem();
    }

    public void tryMoveItems(int selectedMerchantRecipe) {
        if (selectedMerchantRecipe < 0 || selectedMerchantRecipe >= getOffers().size()) {
            return;
        }
        for (int slot = PAYMENT1_SLOT; slot <= PAYMENT2_SLOT; slot++) {
            ItemStack held = tradeContainer.getItem(slot);
            if (!held.isEmpty() && !moveItemStackTo(held, kInventoryStart, kUseRowEnd, true)) {
                return;
            }
            tradeContainer.setItem(slot, held);
        }
        if (tradeContainer.getItem(PAYMENT1_SLOT).isEmpty() && tradeContainer.getItem(PAYMENT2_SLOT).isEmpty()) {
            net.minecraft.world.item.trading.MerchantOffer offer = getOffers().get(selectedMerchantRecipe);
            fillPayment(PAYMENT1_SLOT, offer.getCostA());
            fillPayment(PAYMENT2_SLOT, offer.getCostB());
        }
    }

    private void fillPayment(int paymentSlot, ItemStack cost) {
        if (cost.isEmpty()) {
            return;
        }
        for (int slot = kInventoryStart; slot < kUseRowEnd; slot++) {
            ItemStack inInventory = slots.get(slot).getItem();
            ItemStack inPayment = tradeContainer.getItem(paymentSlot);
            if (inInventory.isEmpty() || !ItemStack.isSameItemSameComponents(cost, inInventory)
                || !inPayment.isEmpty() && !ItemStack.isSameItemSameComponents(inPayment, inInventory)) {
                continue;
            }
            int wanted = cost.getMaxStackSize() - inPayment.getCount();
            int moved = Math.min(wanted, inInventory.getCount());
            ItemStack target = inPayment.isEmpty() ? inInventory.copyWithCount(0) : inPayment;
            target.grow(moved);
            inInventory.shrink(moved);
            tradeContainer.setItem(paymentSlot, target);
            if (target.getCount() >= cost.getMaxStackSize()) {
                break;
            }
        }
    }

    @Override
    public boolean stillValid(Player player) {
        return trader == null || trader.getTradingPlayer() == player;
    }

    public int getTraderXp() {
        return traderXp;
    }

    public void setXp(int xp) {
        this.traderXp = xp;
    }

    public int getTraderLevel() {
        return merchantLevel;
    }

    public void setMerchantLevel(int level) {
        this.merchantLevel = level;
    }

    public void setCanRestock(boolean canRestock) {
        this.canRestock = canRestock;
    }

    public boolean canRestock() {
        return canRestock;
    }

    public void setOffers(MerchantOffers offers) {
        this.offers = offers;
    }

    public MerchantOffers getOffers() {
        return offers;
    }

    @Override
    public boolean canTakeItemForPickAll(ItemStack carried, Slot target) {
        return false;
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        boolean moved;
        if (slotIndex < kInventoryStart) {
            moved = moveItemStackTo(stack, kInventoryStart, kUseRowEnd, true);
        } else if (slotIndex < kUseRowStart) {
            moved = moveItemStackTo(stack, kUseRowStart, kUseRowEnd, false);
        } else {
            moved = moveItemStackTo(stack, kInventoryStart, kInventoryEnd, false);
        }
        if (!moved) {
            return ItemStack.EMPTY;
        }
        if (stack.isEmpty()) {
            slot.setByPlayer(ItemStack.EMPTY);
        } else {
            slot.setChanged();
        }
        return original;
    }

    @Override
    public void removed(Player player) {
        super.removed(player);
        for (int slot = PAYMENT1_SLOT; slot <= PAYMENT2_SLOT; slot++) {
            ItemStack stack = tradeContainer.removeItemNoUpdate(slot);
            if (!stack.isEmpty()) {
                player.getInventory().add(stack);
            }
        }
    }
}
