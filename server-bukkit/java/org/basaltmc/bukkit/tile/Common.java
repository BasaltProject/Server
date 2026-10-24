package org.basaltmc.bukkit.tile;

import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Bukkit;
import org.bukkit.NamespacedKey;
import org.bukkit.inventory.ItemStack;
import org.bukkit.loot.LootTable;

/** The members several kinds share: a custom name, a lock, a loot table. */
final class Common {
    private Common() {}

    private static String nameKey(Tile tile) {
        return tile.block.endsWith("_head") || tile.block.endsWith("_skull") ? "custom_name" : "CustomName";
    }

    private static Map<String, Object> lockWith(Object keyComponent, Map<String, Object> items) {
        Map<String, Object> lock = Nbt.compound();
        if (items != null) {
            lock.putAll(items);
        }
        if (keyComponent != null) {
            Map<String, Object> components = Nbt.compound();
            components.put("minecraft:custom_name", keyComponent);
            lock.put("components", components);
        }
        return lock;
    }

    static Object call(Tile tile, String name, Object[] args) {
        int count = args == null ? 0 : args.length;
        switch (name) {
            case "customName":
                if (count == 0) {
                    return Texts.fromNbt(tile.data.get(nameKey(tile)));
                }
                tile.put(nameKey(tile), args[0] == null ? null : Texts.toNbt((Component) args[0]));
                return null;
            case "getCustomName":
                return count == 0 ? Texts.legacyFromNbt(tile.data.get(nameKey(tile))) : Tile.kNone;
            case "setCustomName":
                if (count == 1) {
                    tile.put(nameKey(tile), args[0] == null ? null : Texts.legacyToNbt((String) args[0]));
                    return null;
                }
                return Tile.kNone;
            case "isLocked":
                return tile.data.get("lock") != null;
            case "getLock": {
                Map<String, Object> lock = tile.map("lock");
                Map<String, Object> components = lock == null ? null : Nbt.map(lock, "components");
                Object key = components == null ? null : components.get("minecraft:custom_name");
                return key == null ? "" : net.kyori.adventure.text.serializer.plain.PlainTextComponentSerializer
                    .plainText().serialize(Texts.fromNbt(key));
            }
            case "setLock":
                tile.put("lock", args[0] == null || ((String) args[0]).isEmpty() ? null
                    : lockWith(Texts.toNbt(Component.text((String) args[0])), null));
                return null;
            case "setLockItem": {
                ItemStack stack = (ItemStack) args[0];
                if (stack == null || stack.getType().isAir()) {
                    tile.put("lock", null);
                    return null;
                }
                Map<String, Object> lock = Nbt.compound();
                lock.put("items", stack.getType().getKey().toString());
                Map<String, Object> components = Nbt.deepCopy(Items.componentsOf(stack));
                if (!components.isEmpty()) {
                    lock.put("components", components);
                }
                tile.put("lock", lock);
                return null;
            }
            case "getLootTable": {
                String key = tile.text("LootTable", null);
                NamespacedKey id = key == null ? null : NamespacedKey.fromString(key);
                return id == null ? null : Bukkit.getLootTable(id);
            }
            case "setLootTable":
                if (args[0] == null) {
                    tile.put("LootTable", null);
                    tile.put("LootTableSeed", null);
                } else {
                    tile.put("LootTable", ((LootTable) args[0]).getKey().toString());
                    if (count == 2) {
                        tile.put("LootTableSeed", (Long) args[1]);
                    }
                }
                return null;
            case "getSeed":
                return count == 0 ? Nbt.longValue(tile.data, "LootTableSeed", 0L) : Tile.kNone;
            case "setSeed":
                if (count == 1) {
                    tile.put("LootTableSeed", (Long) args[0]);
                    return null;
                }
                return Tile.kNone;
            case "isRefillEnabled", "hasBeenFilled", "hasPendingRefill", "hasPlayerLooted":
                return false;
            case "canPlayerLoot":
                return true;
            case "getLastFilled", "getNextRefill":
                return -1L;
            case "getLastLooted":
                return null;
            case "setNextRefill", "setHasPlayerLooted":
                throw Unsupported.hit("LootableInventory#" + name);
            case "getCraftingTicks":
                return tile.integer("crafting_ticks_remaining", 0);
            case "setCraftingTicks":
                tile.put("crafting_ticks_remaining", (Integer) args[0]);
                return null;
            case "isTriggered":
                return tile.integer("triggered", 0) != 0;
            case "setTriggered":
                tile.put("triggered", (Boolean) args[0] ? 1 : 0);
                return null;
            case "isSlotDisabled":
                return tile.data.get("disabled_slots") instanceof int[] slots && contains(slots, (Integer) args[0]);
            case "setSlotDisabled": {
                int[] slots = tile.data.get("disabled_slots") instanceof int[] held ? held : new int[0];
                java.util.Set<Integer> set = new java.util.TreeSet<>();
                for (int slot : slots) {
                    set.add(slot);
                }
                if ((Boolean) args[1]) {
                    set.add((Integer) args[0]);
                } else {
                    set.remove((Integer) args[0]);
                }
                tile.put("disabled_slots", set.stream().mapToInt(Integer::intValue).toArray());
                return null;
            }
            case "getCookSpeedMultiplier":
                return Nbt.real(tile.data, "Paper.CookSpeedMultiplier", 1.0);
            case "setCookSpeedMultiplier": {
                double multiplier = (Double) args[0];
                if (multiplier < 0.0 || multiplier > 200.0) {
                    throw new IllegalArgumentException("Furnace speed multiplier must be between 0 and 200");
                }
                tile.put("Paper.CookSpeedMultiplier", multiplier);
                tile.put("Basalt.CookSpeedChanged", (byte) 1);
                return null;
            }
            case "getRecipeUsedCount":
                return Nbt.integer(Nbt.mapOrCreate(tile.data, "RecipesUsed"), args[0].toString(), 0);
            case "hasRecipeUsedCount":
                return Nbt.mapOrCreate(tile.data, "RecipesUsed").containsKey(args[0].toString());
            case "setRecipeUsedCount": {
                org.bukkit.inventory.CookingRecipe<?> recipe = (org.bukkit.inventory.CookingRecipe<?>) args[0];
                Map<String, Object> used = Nbt.mapOrCreate(tile.data, "RecipesUsed");
                if ((Integer) args[1] <= 0) {
                    used.remove(recipe.getKey().toString());
                } else {
                    used.put(recipe.getKey().toString(), args[1]);
                }
                return null;
            }
            case "getRecipesUsed": {
                Map<org.bukkit.inventory.CookingRecipe<?>, Integer> out = new java.util.LinkedHashMap<>();
                for (Map.Entry<String, Object> entry : Nbt.mapOrCreate(tile.data, "RecipesUsed").entrySet()) {
                    NamespacedKey key = NamespacedKey.fromString(entry.getKey());
                    if (key != null && Bukkit.getRecipe(key) instanceof org.bukkit.inventory.CookingRecipe<?> recipe) {
                        out.put(recipe, ((Number) entry.getValue()).intValue());
                    }
                }
                return out;
            }
            case "setRecipesUsed": {
                Map<String, Object> used = Nbt.compound();
                for (Map.Entry<org.bukkit.inventory.CookingRecipe<?>, Integer> entry
                    : ((Map<org.bukkit.inventory.CookingRecipe<?>, Integer>) args[0]).entrySet()) {
                    used.put(entry.getKey().getKey().toString(), entry.getValue());
                }
                tile.put("RecipesUsed", used);
                return null;
            }
            case "dispense", "drop":
                if (args == null || args.length == 0) {
                    return dispense(tile);
                }
                return Tile.kNone;
            case "open":
                return Lids.set(tile, true);
            case "close":
                return Lids.set(tile, false);
            case "isOpen":
                return Lids.isOpen(tile);
            default:
                return Tile.kNone;
        }
    }

    private static boolean contains(int[] slots, int slot) {
        for (int value : slots) {
            if (value == slot) {
                return true;
            }
        }
        return false;
    }

    private static boolean dispense(Tile tile) {
        List<Object> items = Nbt.list(tile.data, "Items");
        if (items == null || items.isEmpty()) {
            return false;
        }
        return org.basaltmc.bukkit.WorldNatives.scheduleTick(tile.world.getName(), tile.x, tile.y, tile.z, 0, false);
    }
}
