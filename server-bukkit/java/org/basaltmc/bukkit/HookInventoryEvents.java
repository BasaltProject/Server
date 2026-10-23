package org.basaltmc.bukkit;

/** The inventory events raised from deep in the game systems, as the Bukkit events plugins listen for. */
final class HookInventoryEvents {
    private HookInventoryEvents() {}

    static void install() {
        EventBridge.map("inventory.trade", org.bukkit.event.inventory.TradeSelectEvent.class, f -> {
            org.bukkit.entity.Player who = f.player();
            org.bukkit.inventory.InventoryView view = who == null ? null : org.basaltmc.bukkit.inv.Windows.current(who);
            return view instanceof org.bukkit.inventory.view.MerchantView merchant
                ? new org.bukkit.event.inventory.TradeSelectEvent(merchant, (int) f.whole("index")) : null;
        });
        EventBridge.map("inventory.furnace_extract", org.bukkit.event.inventory.FurnaceExtractEvent.class, f -> {
            org.bukkit.Material material = org.bukkit.Material.matchMaterial(f.text("item"));
            return f.player() == null || material == null ? null
                : new org.bukkit.event.inventory.FurnaceExtractEvent(f.player(), f.world().getBlockAt((int) f.whole("x"),
                    (int) f.whole("y"), (int) f.whole("z")), new org.bukkit.inventory.ItemStack(material,
                    (int) f.whole("amount")), (int) f.whole("amount"), (int) f.whole("experience"));
        }, (e, f) -> f.put("experience", (long) ((org.bukkit.event.inventory.FurnaceExtractEvent) e).getExpToDrop()));
        EventBridge.map("inventory.creative", org.bukkit.event.inventory.InventoryCreativeEvent.class, f -> {
            org.bukkit.entity.Player who = f.player();
            if (who == null) {
                return null;
            }
            int raw = (int) f.whole("slot");
            org.bukkit.Material material = f.text("item").isEmpty() ? null
                : org.bukkit.Material.matchMaterial(f.text("item"));
            org.bukkit.inventory.ItemStack item = material == null ? new org.bukkit.inventory.ItemStack(
                org.bukkit.Material.AIR) : new org.bukkit.inventory.ItemStack(material, (int) f.whole("count"));
            org.bukkit.inventory.InventoryView view = who.getOpenInventory();
            return new org.bukkit.event.inventory.InventoryCreativeEvent(view, view.getSlotType(raw), raw, item);
        }, (e, f) -> {
            org.bukkit.inventory.ItemStack item = ((org.bukkit.event.inventory.InventoryCreativeEvent) e).getCursor();
            f.put("item", item.getType().isAir() ? "" : item.getType().getKey().toString());
            f.put("count", (long) item.getAmount());
        });
    }
}
