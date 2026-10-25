package org.basaltmc.bukkit;

import java.util.LinkedHashMap;
import java.util.Map;
import org.basaltmc.bukkit.inv.BasaltInventoryView;
import org.basaltmc.bukkit.inv.PrepareViews;
import org.basaltmc.bukkit.inv.TypedInventories;
import org.basaltmc.bukkit.inv.Windows;
import org.bukkit.Material;
import org.bukkit.entity.Player;
import org.bukkit.event.Event;
import org.bukkit.event.inventory.ClickType;
import org.bukkit.event.inventory.InventoryAction;
import org.bukkit.event.inventory.InventoryClickEvent;
import org.bukkit.event.inventory.InventoryCloseEvent;
import org.bukkit.event.inventory.InventoryDragEvent;
import org.bukkit.event.inventory.InventoryOpenEvent;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryView;
import org.bukkit.inventory.ItemStack;

final class InventoryEvents {
    private static final int kOutside = -999;

    private InventoryEvents() {}

    private record Click(BasaltInventoryView view, InventoryType.SlotType slotType, int raw, ClickType click,
            InventoryAction action, int key) {}

    private static final Map<String, Click> lastClick = new java.util.HashMap<>();
    private static final Map<String, org.bukkit.inventory.Recipe> recipeCache = new java.util.HashMap<>();

    static void register() {
        EventBridge.map("inventory.smelt", org.bukkit.event.inventory.FurnaceSmeltEvent.class, f -> {
            ItemStack source = Items.fromSnbt(f.text("source"));
            ItemStack output = Items.fromSnbt(f.text("output"));
            if (source == null || output == null) {
                return null;
            }
            org.bukkit.inventory.Recipe recipe = recipeOf(f.text("recipe"));
            return new org.bukkit.event.inventory.FurnaceSmeltEvent(f.block(), source, output,
                recipe instanceof org.bukkit.inventory.CookingRecipe<?> cooking ? cooking : null);
        }, (e, f) -> f.put("output", Items.toSnbt(((org.bukkit.event.inventory.FurnaceSmeltEvent) e).getResult())));
        EventBridge.map("furnace.burn", org.bukkit.event.inventory.FurnaceBurnEvent.class, f -> {
            ItemStack fuel = Items.fromSnbt(f.text("fuel"));
            return fuel == null ? null : new org.bukkit.event.inventory.FurnaceBurnEvent(f.block(), fuel,
                (int) f.whole("burn_time"));
        }, (e, f) -> f.put("burn_time", (long) ((org.bukkit.event.inventory.FurnaceBurnEvent) e).getBurnTime()));
        EventBridge.map("furnace.start", org.bukkit.event.inventory.FurnaceStartSmeltEvent.class, f -> {
            ItemStack source = Items.fromSnbt(f.text("source"));
            org.bukkit.inventory.Recipe recipe = recipeOf(f.text("recipe"));
            return source == null || !(recipe instanceof org.bukkit.inventory.CookingRecipe<?> cooking) ? null
                : new org.bukkit.event.inventory.FurnaceStartSmeltEvent(f.block(), source, cooking,
                    (int) f.whole("total"));
        });
        EventBridge.map("inventory.enchant_prepare", org.bukkit.event.enchantment.PrepareItemEnchantEvent.class,
            InventoryEvents::prepareEnchant, InventoryEvents::enchantOffersBack);
        EventBridge.map("inventory.enchant", org.bukkit.event.enchantment.EnchantItemEvent.class,
            InventoryEvents::enchantItem, InventoryEvents::enchantBack);
        EventBridge.map("inventory.prepare", org.bukkit.event.inventory.PrepareItemCraftEvent.class,
            f -> f.text("kind").equals("crafting") ? prepareCraft(f) : null, InventoryEvents::resultBack);
        EventBridge.map("inventory.prepare", org.bukkit.event.inventory.PrepareAnvilEvent.class,
            f -> f.text("kind").equals("anvil") ? prepareAnvil(f) : null, InventoryEvents::anvilBack);
        EventBridge.map("inventory.prepare", org.bukkit.event.inventory.PrepareSmithingEvent.class,
            f -> f.text("kind").equals("smithing") ? prepareSmithing(f) : null, InventoryEvents::resultBack);
        EventBridge.map("inventory.take", org.bukkit.event.inventory.CraftItemEvent.class,
            f -> f.text("kind").equals("crafting") ? takeCraft(f) : null);
        EventBridge.map("inventory.take", org.bukkit.event.inventory.SmithItemEvent.class,
            f -> f.text("kind").equals("smithing") ? takeSmith(f) : null);
        EventBridge.map("inventory.open", InventoryOpenEvent.class, f -> {
            Player player = f.player();
            BasaltInventoryView view = player == null ? null : Windows.pending(player, f.text("kind"),
                f.text("world"), (int) f.whole("x"), (int) f.whole("y"), (int) f.whole("z"));
            return view == null ? null : new InventoryOpenEvent(view);
        });
        EventBridge.map("inventory.click", InventoryClickEvent.class, InventoryEvents::click,
            InventoryEvents::afterClick);
        EventBridge.map("inventory.drag", InventoryDragEvent.class, InventoryEvents::drag);
        EventBridge.map("inventory.close", InventoryCloseEvent.class, f -> {
            Player player = f.player();
            BasaltInventoryView view = player == null ? null : Windows.cached(player);
            if (view == null) {
                view = player == null ? null : Windows.current(player);
            }
            return view == null ? null : new InventoryCloseEvent(view, Windows.reasonOf(f.text("reason")));
        }, (e, f) -> Windows.closed(((InventoryCloseEvent) e).getPlayer()));
        EventBridge.mapAlways("inventory.changed", f -> {
            Player player = f.player();
            if (player != null) {
                Windows.synchronize(player);
            }
        });
    }

    private static boolean empty(ItemStack item) {
        return item == null || item.getType().isAir() || item.getAmount() <= 0;
    }

    private static Event click(EventBridge.Fields f) {
        String mode = f.text("mode");
        if (mode.equals("quick_craft")) {
            return null;
        }
        Player player = f.player();
        BasaltInventoryView view = player == null ? null : Windows.current(player);
        if (view == null) {
            return null;
        }
        int raw = (int) f.whole("slot");
        int button = (int) f.whole("button");
        InventoryType.SlotType slotType = view.getSlotType(raw);
        ItemStack cursor = view.getCursor();
        ItemStack clicked = raw == kOutside ? null : view.getItem(raw);
        ClickType click = clickType(mode, button, raw);
        InventoryAction action = action(view, mode, button, raw, slotType, clicked, cursor);
        int key = mode.equals("swap") && button >= 0 && button < 9 ? button : -1;
        lastClick.put(f.text("player_uuid"), new Click(view, slotType, raw, click, action, key));
        InventoryType kind = view.getType();
        if (slotType == InventoryType.SlotType.RESULT && !empty(clicked)
            && (kind == InventoryType.CRAFTING || kind == InventoryType.WORKBENCH || kind == InventoryType.SMITHING)) {
            return null;
        }
        return key >= 0 ? new InventoryClickEvent(view, slotType, raw, click, action, key)
            : new InventoryClickEvent(view, slotType, raw, click, action);
    }

    private static void afterClick(Event event, EventBridge.Fields f) {
        InventoryClickEvent click = (InventoryClickEvent) event;
        if (click.isCancelled()) {
            return;
        }
        InventoryView view = click.getView();
        ItemStack current = click.getCurrentItem();
        int raw = click.getRawSlot();
        if (raw != kOutside && current != view.getItem(raw) && !sameStack(current, view.getItem(raw))) {
            view.setItem(raw, current);
        }
    }

    private static boolean sameStack(ItemStack a, ItemStack b) {
        return empty(a) ? empty(b) : a.equals(b);
    }

    private static ClickType clickType(String mode, int button, int raw) {
        switch (mode) {
            case "pickup":
                if (raw == kOutside) {
                    return button == 0 ? ClickType.WINDOW_BORDER_LEFT : ClickType.WINDOW_BORDER_RIGHT;
                }
                return button == 0 ? ClickType.LEFT : ClickType.RIGHT;
            case "quick_move":
                return button == 0 ? ClickType.SHIFT_LEFT : ClickType.SHIFT_RIGHT;
            case "swap":
                return button == 40 ? ClickType.SWAP_OFFHAND : ClickType.NUMBER_KEY;
            case "clone":
                return ClickType.MIDDLE;
            case "throw":
                return button == 0 ? ClickType.DROP : ClickType.CONTROL_DROP;
            case "pickup_all":
                return ClickType.DOUBLE_CLICK;
            default:
                return ClickType.UNKNOWN;
        }
    }

    private static InventoryAction action(InventoryView view, String mode, int button, int raw,
            InventoryType.SlotType slotType, ItemStack clicked, ItemStack cursor) {
        boolean slotEmpty = empty(clicked);
        boolean cursorEmpty = empty(cursor);
        switch (mode) {
            case "pickup":
                if (raw == kOutside) {
                    if (cursorEmpty) {
                        return InventoryAction.NOTHING;
                    }
                    return button == 0 ? InventoryAction.DROP_ALL_CURSOR : InventoryAction.DROP_ONE_CURSOR;
                }
                if (slotType == InventoryType.SlotType.RESULT) {
                    if (slotEmpty) {
                        return InventoryAction.NOTHING;
                    }
                    return cursorEmpty || cursor.isSimilar(clicked) ? InventoryAction.PICKUP_ALL : InventoryAction.NOTHING;
                }
                if (slotEmpty && cursorEmpty) {
                    return InventoryAction.NOTHING;
                }
                if (button == 0) {
                    if (cursorEmpty) {
                        return InventoryAction.PICKUP_ALL;
                    }
                    if (slotEmpty) {
                        return InventoryAction.PLACE_ALL;
                    }
                    if (cursor.isSimilar(clicked)) {
                        int room = Math.min(clicked.getMaxStackSize(), view.getTopInventory().getMaxStackSize()) - clicked.getAmount();
                        return cursor.getAmount() <= room ? InventoryAction.PLACE_ALL : room > 0 ? InventoryAction.PLACE_SOME : InventoryAction.NOTHING;
                    }
                    return InventoryAction.SWAP_WITH_CURSOR;
                }
                if (cursorEmpty) {
                    return InventoryAction.PICKUP_HALF;
                }
                if (slotEmpty) {
                    return InventoryAction.PLACE_ONE;
                }
                if (cursor.isSimilar(clicked)) {
                    return clicked.getAmount() < clicked.getMaxStackSize() ? InventoryAction.PLACE_ONE : InventoryAction.NOTHING;
                }
                return InventoryAction.SWAP_WITH_CURSOR;
            case "quick_move":
                return slotEmpty ? InventoryAction.NOTHING : InventoryAction.MOVE_TO_OTHER_INVENTORY;
            case "swap": {
                ItemStack hotbar = button == 40 ? view.getBottomInventory().getItem(40)
                    : view.getBottomInventory().getItem(button);
                if (slotEmpty && empty(hotbar)) {
                    return InventoryAction.NOTHING;
                }
                return slotType == InventoryType.SlotType.RESULT ? InventoryAction.HOTBAR_MOVE_AND_READD
                    : InventoryAction.HOTBAR_SWAP;
            }
            case "clone":
                return slotEmpty ? InventoryAction.NOTHING : InventoryAction.CLONE_STACK;
            case "throw":
                if (slotEmpty) {
                    return InventoryAction.NOTHING;
                }
                return button == 0 ? InventoryAction.DROP_ONE_SLOT : InventoryAction.DROP_ALL_SLOT;
            case "pickup_all":
                return InventoryAction.COLLECT_TO_CURSOR;
            default:
                return InventoryAction.UNKNOWN;
        }
    }

    private static Event drag(EventBridge.Fields f) {
        Player player = f.player();
        BasaltInventoryView view = player == null ? null : Windows.current(player);
        if (view == null) {
            return null;
        }
        boolean single = f.text("type").equals("single");
        ItemStack cursor = view.getCursor();
        if (empty(cursor)) {
            return null;
        }
        String[] named = f.text("slots").isEmpty() ? new String[0] : f.text("slots").split(",");
        int count = named.length;
        int each = single ? 1 : Math.max(1, cursor.getAmount() / Math.max(1, count));
        int remaining = cursor.getAmount();
        Map<Integer, ItemStack> placed = new LinkedHashMap<>();
        for (String name : named) {
            int slot = Integer.parseInt(name);
            ItemStack there = view.getItem(slot);
            if (!empty(there) && !there.isSimilar(cursor)) {
                continue;
            }
            int have = empty(there) ? 0 : there.getAmount();
            int add = Math.min(Math.min(each, remaining), cursor.getMaxStackSize() - have);
            if (add <= 0) {
                continue;
            }
            ItemStack grown = cursor.clone();
            grown.setAmount(have + add);
            placed.put(slot, grown);
            remaining -= add;
        }
        ItemStack newCursor = cursor.clone();
        newCursor.setAmount(remaining);
        return new InventoryDragEvent(view, remaining > 0 ? newCursor : null, cursor, single, placed);
    }

    private static org.bukkit.inventory.Recipe recipeOf(String id) {
        if (id.isEmpty()) {
            return null;
        }
        return org.basaltmc.bukkit.facade.Memo.get(recipeCache, id, key -> org.basaltmc.bukkit.recipe.Recipes.get(
            org.bukkit.NamespacedKey.fromString(key)));
    }

    private static final String kResultKey = "result";

    private static Event prepareCraft(EventBridge.Fields f) {
        Player player = f.player();
        BasaltInventoryView window = player == null ? null : Windows.current(player);
        if (window == null) {
            return null;
        }
        org.bukkit.inventory.Recipe recipe = recipeOf(f.text("recipe"));
        org.basaltmc.bukkit.inv.SnapshotInventory snapshot = PrepareViews.copyOf(window,
            window.getTopInventory().getType(), PrepareViews.kResultCraft, Items.fromSnbt(f.text(kResultKey)));
        org.bukkit.inventory.Inventory typed = TypedInventories.wrap(snapshot, recipe, Map.of());
        BasaltInventoryView view = PrepareViews.view(player, typed, window);
        return new org.bukkit.event.inventory.PrepareItemCraftEvent((org.bukkit.inventory.CraftingInventory) typed,
            view, f.text("recipe").contains("repair"));
    }

    private static Event prepareAnvil(EventBridge.Fields f) {
        Player player = f.player();
        BasaltInventoryView window = player == null ? null : Windows.current(player);
        if (window == null) {
            return null;
        }
        Map<String, Object> state = PrepareViews.Anvil.state(f.text("name"), (int) f.whole("cost"));
        ItemStack result = Items.fromSnbt(f.text(kResultKey));
        org.basaltmc.bukkit.inv.SnapshotInventory snapshot = PrepareViews.copyOf(window, InventoryType.ANVIL,
            PrepareViews.kResultAnvil, result);
        org.bukkit.inventory.Inventory typed = TypedInventories.wrap(snapshot, null, state);
        PrepareViews.Anvil view = new PrepareViews.Anvil(player, typed, window, state);
        anvilStates.put(f.text("player_uuid"), state);
        return new org.bukkit.event.inventory.PrepareAnvilEvent(view, result);
    }

    private static final Map<String, Map<String, Object>> anvilStates = new java.util.HashMap<>();

    private static Event prepareSmithing(EventBridge.Fields f) {
        Player player = f.player();
        BasaltInventoryView window = player == null ? null : Windows.current(player);
        if (window == null) {
            return null;
        }
        ItemStack result = Items.fromSnbt(f.text(kResultKey));
        org.basaltmc.bukkit.inv.SnapshotInventory snapshot = PrepareViews.copyOf(window, InventoryType.SMITHING,
            PrepareViews.kResultSmithing, result);
        org.bukkit.inventory.Inventory typed = TypedInventories.wrap(snapshot, recipeOf(f.text("recipe")), Map.of());
        return new org.bukkit.event.inventory.PrepareSmithingEvent(PrepareViews.view(player, typed, window), result);
    }

    private static void resultBack(Event event, EventBridge.Fields f) {
        ItemStack result = event instanceof com.destroystokyo.paper.event.inventory.PrepareResultEvent prepared
            ? prepared.getResult()
            : ((org.bukkit.event.inventory.PrepareItemCraftEvent) event).getInventory().getResult();
        f.put(kResultKey, Items.toSnbt(result));
    }

    private static void anvilBack(Event event, EventBridge.Fields f) {
        org.bukkit.event.inventory.PrepareAnvilEvent anvil = (org.bukkit.event.inventory.PrepareAnvilEvent) event;
        f.put(kResultKey, Items.toSnbt(anvil.getResult()));
        f.put("cost", (long) anvil.getView().getRepairCost());
    }

    private static Event takeCraft(EventBridge.Fields f) {
        Click click = lastClick.get(f.text("player_uuid"));
        if (click == null) {
            return null;
        }
        org.bukkit.inventory.Recipe recipe = recipeOf(f.text("recipe"));
        return click.key() >= 0
            ? new org.bukkit.event.inventory.CraftItemEvent(recipe, click.view(), click.slotType(), click.raw(),
                click.click(), click.action(), click.key())
            : new org.bukkit.event.inventory.CraftItemEvent(recipe, click.view(), click.slotType(), click.raw(),
                click.click(), click.action());
    }

    private static Event takeSmith(EventBridge.Fields f) {
        Click click = lastClick.get(f.text("player_uuid"));
        if (click == null) {
            return null;
        }
        return click.key() >= 0
            ? new org.bukkit.event.inventory.SmithItemEvent(click.view(), click.slotType(), click.raw(),
                click.click(), click.action(), click.key())
            : new org.bukkit.event.inventory.SmithItemEvent(click.view(), click.slotType(), click.raw(),
                click.click(), click.action());
    }

    private static org.bukkit.enchantments.EnchantmentOffer[] parseOffers(String text) {
        org.bukkit.enchantments.EnchantmentOffer[] offers = new org.bukkit.enchantments.EnchantmentOffer[3];
        String[] rows = text.split(";", -1);
        for (int at = 0; at < 3 && at < rows.length; at++) {
            String[] parts = rows[at].split("\\|");
            if (parts.length == 3) {
                org.bukkit.NamespacedKey key = org.bukkit.NamespacedKey.fromString(parts[0]);
                org.bukkit.enchantments.Enchantment enchantment = key == null ? null
                    : org.bukkit.enchantments.Enchantment.getByKey(key);
                if (enchantment != null) {
                    offers[at] = new org.bukkit.enchantments.EnchantmentOffer(enchantment,
                        Integer.parseInt(parts[1]), Integer.parseInt(parts[2]));
                }
            }
        }
        return offers;
    }

    private static Event prepareEnchant(EventBridge.Fields f) {
        Player player = f.player();
        BasaltInventoryView window = player == null ? null : Windows.current(player);
        ItemStack item = Items.fromSnbt(f.text("item"));
        if (window == null || item == null) {
            return null;
        }
        org.bukkit.enchantments.EnchantmentOffer[] offers = parseOffers(f.text("offers"));
        org.basaltmc.bukkit.inv.SnapshotInventory snapshot = PrepareViews.copyOf(window, InventoryType.ENCHANTING,
            Integer.MAX_VALUE, null);
        org.bukkit.inventory.Inventory typed = TypedInventories.wrap(snapshot, null, Map.of());
        PrepareViews.Enchanting view = new PrepareViews.Enchanting(player, typed, window, offers);
        return new org.bukkit.event.enchantment.PrepareItemEnchantEvent(player, view, f.block(), item, offers,
            (int) f.whole("bookshelves"));
    }

    private static void enchantOffersBack(Event event, EventBridge.Fields f) {
        org.bukkit.event.enchantment.PrepareItemEnchantEvent prepared =
            (org.bukkit.event.enchantment.PrepareItemEnchantEvent) event;
        StringBuilder out = new StringBuilder();
        org.bukkit.enchantments.EnchantmentOffer[] offers = prepared.getOffers();
        for (int at = 0; at < 3; at++) {
            if (at > 0) {
                out.append(';');
            }
            org.bukkit.enchantments.EnchantmentOffer offer = at < offers.length ? offers[at] : null;
            if (offer != null) {
                out.append(offer.getEnchantment().getKey()).append('|').append(offer.getEnchantmentLevel()).append('|')
                    .append(offer.getCost());
            }
        }
        f.put("offers", out.toString());
    }

    private static Event enchantItem(EventBridge.Fields f) {
        Player player = f.player();
        BasaltInventoryView window = player == null ? null : Windows.current(player);
        ItemStack item = Items.fromSnbt(f.text("item"));
        if (window == null || item == null) {
            return null;
        }
        Map<org.bukkit.enchantments.Enchantment, Integer> chosen = new LinkedHashMap<>();
        for (String entry : f.text("enchants").split(",")) {
            int colon = entry.lastIndexOf(':');
            org.bukkit.NamespacedKey key = colon < 0 ? null : org.bukkit.NamespacedKey.fromString(entry.substring(0, colon));
            org.bukkit.enchantments.Enchantment enchantment = key == null ? null
                : org.bukkit.enchantments.Enchantment.getByKey(key);
            if (enchantment != null) {
                chosen.put(enchantment, Integer.parseInt(entry.substring(colon + 1)));
            }
        }
        java.util.Map.Entry<org.bukkit.enchantments.Enchantment, Integer> first = chosen.entrySet().stream().findFirst().orElse(null);
        return new org.bukkit.event.enchantment.EnchantItemEvent(player, window, f.block(), item,
            (int) f.whole("cost"), chosen, first == null ? null : first.getKey(), first == null ? 0 : first.getValue(),
            (int) f.whole("button"));
    }

    private static void enchantBack(Event event, EventBridge.Fields f) {
        org.bukkit.event.enchantment.EnchantItemEvent enchanted = (org.bukkit.event.enchantment.EnchantItemEvent) event;
        StringBuilder out = new StringBuilder();
        for (Map.Entry<org.bukkit.enchantments.Enchantment, Integer> entry : enchanted.getEnchantsToAdd().entrySet()) {
            if (out.length() > 0) {
                out.append(',');
            }
            out.append(entry.getKey().getKey()).append(':').append(entry.getValue());
        }
        f.put("enchants", out.toString());
        f.put("cost", (long) enchanted.getExpLevelCost());
    }
}
