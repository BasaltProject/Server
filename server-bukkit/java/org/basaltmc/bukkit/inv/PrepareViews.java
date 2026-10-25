package org.basaltmc.bukkit.inv;

import java.util.HashMap;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.view.AnvilView;

public final class PrepareViews {
    private PrepareViews() {}

    public static final int kResultCraft = 0;
    public static final int kResultAnvil = 2;
    public static final int kResultSmithing = 3;

    /** The top of an open window copied slot by slot, with the result slot set to what the server worked out. */
    public static SnapshotInventory copyOf(BasaltInventoryView window, InventoryType type, int resultSlot,
            ItemStack result) {
        Inventory top = window.getTopInventory();
        ItemStack[] slots = new ItemStack[top.getSize()];
        for (int slot = 0; slot < slots.length; slot++) {
            slots[slot] = top.getItem(slot);
        }
        if (resultSlot < slots.length) {
            slots[resultSlot] = result;
        }
        return new SnapshotInventory(type, slots).withViewers(top.getViewers());
    }

    public static BasaltInventoryView view(HumanEntity player, Inventory top, BasaltInventoryView window) {
        return new BasaltInventoryView(player, top, Component.empty(), window.playerWindow(), window.containerId());
    }

    /** An anvil view whose rename text and costs are what the server has now and what a listener may change. */
    public static final class Anvil extends BasaltInventoryView implements AnvilView {
        private final Map<String, Object> state;

        public Anvil(HumanEntity player, Inventory top, BasaltInventoryView window, Map<String, Object> state) {
            super(player, top, Component.empty(), false, window.containerId());
            this.state = state;
        }

        public static Map<String, Object> state(String rename, int cost) {
            Map<String, Object> state = new HashMap<>();
            state.put("rename", rename);
            state.put("cost", cost);
            state.put("maximum", 40);
            state.put("amount", 0);
            state.put("bypass", false);
            return state;
        }

        @Override
        public org.bukkit.inventory.AnvilInventory getTopInventory() {
            return (org.bukkit.inventory.AnvilInventory) super.getTopInventory();
        }

        @Override
        public String getRenameText() {
            String text = (String) state.get("rename");
            return text == null || text.isEmpty() ? null : text;
        }

        @Override
        public int getRepairItemCountCost() {
            return (Integer) state.get("amount");
        }

        @Override
        public int getRepairCost() {
            return (Integer) state.get("cost");
        }

        @Override
        public int getMaximumRepairCost() {
            return (Integer) state.get("maximum");
        }

        @Override
        public void setRepairItemCountCost(int cost) {
            state.put("amount", cost);
        }

        @Override
        public void setRepairCost(int cost) {
            state.put("cost", cost);
        }

        @Override
        public void setMaximumRepairCost(int cost) {
            state.put("maximum", cost);
        }

        @Override
        public boolean bypassesEnchantmentLevelRestriction() {
            return (Boolean) state.get("bypass");
        }

        @Override
        public void bypassEnchantmentLevelRestriction(boolean bypass) {
            state.put("bypass", bypass);
        }
    }

    public static final class Enchanting extends BasaltInventoryView implements org.bukkit.inventory.view.EnchantmentView {
        private org.bukkit.enchantments.EnchantmentOffer[] offers;
        private int seed;

        public Enchanting(HumanEntity player, Inventory top, BasaltInventoryView window,
                org.bukkit.enchantments.EnchantmentOffer[] offers) {
            super(player, top, Component.empty(), false, window.containerId());
            this.offers = offers;
        }

        @Override
        public org.bukkit.inventory.EnchantingInventory getTopInventory() {
            return (org.bukkit.inventory.EnchantingInventory) super.getTopInventory();
        }

        @Override
        public int getEnchantmentSeed() {
            return seed;
        }

        @Override
        public void setEnchantmentSeed(int seed) {
            this.seed = seed;
        }

        @Override
        public org.bukkit.enchantments.EnchantmentOffer[] getOffers() {
            return offers;
        }

        @Override
        public void setOffers(org.bukkit.enchantments.EnchantmentOffer[] offers) {
            if (offers.length != 3) {
                throw new IllegalArgumentException("There must be three offers");
            }
            this.offers = offers;
        }
    }
}
