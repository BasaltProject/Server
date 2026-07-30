package org.basaltmc.bukkit.inv;

import net.kyori.adventure.text.Component;
import org.bukkit.entity.HumanEntity;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.Merchant;

/** A merchant window that has not been opened yet: opening it hands the merchant to the player. */
final class MerchantView extends BasaltInventoryView {
    private final Merchant merchant;

    MerchantView(HumanEntity player, Inventory top, Component title, Merchant merchant) {
        super(player, top, title, false, -1);
        this.merchant = merchant;
    }

    Merchant merchant() {
        return merchant;
    }
}
