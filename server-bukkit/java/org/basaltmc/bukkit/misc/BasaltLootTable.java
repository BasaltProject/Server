package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Random;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.ItemStack;
import org.bukkit.loot.LootContext;
import org.bukkit.loot.LootTable;

public final class BasaltLootTable implements LootTable {
    private final NamespacedKey key;

    private BasaltLootTable(NamespacedKey key) {
        this.key = key;
    }

    public static LootTable get(NamespacedKey key) {
        return MiscNatives.lootHas(key.toString()) ? new BasaltLootTable(key) : null;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public Collection<ItemStack> populateLoot(Random random, LootContext context) {
        Location at = context.getLocation();
        if (at == null || at.getWorld() == null) {
            throw new IllegalArgumentException("The loot context needs a location in a world");
        }
        String[] flat = MiscNatives.lootRoll(key.toString(), at.getWorld().getName(), at.getX(), at.getY(), at.getZ(),
            context.getLuck(), random == null ? 0L : random.nextLong(),
            context.getKiller() == null ? null : context.getKiller().getUniqueId().toString());
        List<ItemStack> out = new ArrayList<>();
        for (int i = 0; flat != null && i + 2 < flat.length; i += 3) {
            Material material = Material.matchMaterial(flat[i]);
            if (material != null && !material.isAir()) {
                out.add(new ItemStack(material, Integer.parseInt(flat[i + 1])));
            }
        }
        return out;
    }

    @Override
    public void fillInventory(Inventory inventory, Random random, LootContext context) {
        Random rolled = random == null ? new Random() : random;
        List<ItemStack> items = new ArrayList<>(populateLoot(rolled, context));
        List<Integer> empty = new ArrayList<>();
        for (int slot = 0; slot < inventory.getSize(); slot++) {
            ItemStack held = inventory.getItem(slot);
            if (held == null || held.getType().isAir()) {
                empty.add(slot);
            }
        }
        List<ItemStack> splittable = new ArrayList<>();
        List<ItemStack> results = new ArrayList<>();
        for (ItemStack item : items) {
            (item.getAmount() > 1 ? splittable : results).add(item);
        }
        while (empty.size() - results.size() - splittable.size() > 0 && !splittable.isEmpty()) {
            ItemStack stack = splittable.remove(rolled.nextInt(splittable.size()));
            int split = 1 + rolled.nextInt(Math.max(1, stack.getAmount() / 2));
            ItemStack part = stack.clone();
            part.setAmount(split);
            stack.setAmount(stack.getAmount() - split);
            if (stack.getAmount() > 1 && rolled.nextBoolean()) {
                splittable.add(stack);
            } else {
                results.add(stack);
            }
            if (part.getAmount() > 1 && rolled.nextBoolean()) {
                splittable.add(part);
            } else {
                results.add(part);
            }
        }
        results.addAll(splittable);
        for (int i = results.size(); i > 1; i--) {
            java.util.Collections.swap(results, i - 1, rolled.nextInt(i));
        }
        for (int i = empty.size(); i > 1; i--) {
            java.util.Collections.swap(empty, i - 1, rolled.nextInt(i));
        }
        for (ItemStack result : results) {
            if (empty.isEmpty()) {
                break;
            }
            inventory.setItem(empty.remove(empty.size() - 1), result);
        }
    }
}
