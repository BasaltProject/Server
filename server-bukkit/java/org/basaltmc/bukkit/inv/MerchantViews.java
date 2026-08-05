package org.basaltmc.bukkit.inv;

import org.bukkit.entity.HumanEntity;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.Merchant;
import org.bukkit.inventory.MerchantInventory;
import org.bukkit.inventory.view.MerchantView;

/** A merchant window as the trade a player picked sees it. */
public final class MerchantViews extends BasaltInventoryView implements MerchantView {
    private final Merchant merchant;

    public MerchantViews(HumanEntity player, Inventory top, BasaltInventoryView window, Merchant merchant) {
        super(player, top, window.title(), false, window.containerId());
        this.merchant = merchant;
    }

    @Override
    public MerchantInventory getTopInventory() {
        return (MerchantInventory) top;
    }

    @Override
    public Merchant getMerchant() {
        return merchant;
    }
}
