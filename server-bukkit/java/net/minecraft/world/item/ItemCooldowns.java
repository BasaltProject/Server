// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;

public class ItemCooldowns {
    private final org.bukkit.entity.HumanEntity owner;
    private final java.util.Map<Identifier, int[]> local = new java.util.HashMap<>();
    private int tickCount;

    public ItemCooldowns() {
        this(null);
    }

    public ItemCooldowns(org.bukkit.entity.HumanEntity owner) {
        this.owner = owner;
    }

    private static org.bukkit.inventory.ItemStack mirror(ItemStack item) {
        return org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(item);
    }

    public boolean isOnCooldown(ItemStack item) {
        return getCooldownPercent(item, 0.0F) > 0.0F;
    }

    public float getCooldownPercent(ItemStack item, float a) {
        if (owner != null) {
            return owner.hasCooldown(mirror(item)) ? 1.0F : 0.0F;
        }
        int[] window = local.get(getCooldownGroup(item));
        if (window == null || tickCount >= window[1]) {
            return 0.0F;
        }
        return Math.min((window[1] - (tickCount + a)) / (float) (window[1] - window[0]), 1.0F);
    }

    public void tick() {
        tickCount++;
        local.entrySet().removeIf(entry -> {
            boolean ended = tickCount >= entry.getValue()[1];
            if (ended) {
                onCooldownEnded(entry.getKey());
            }
            return ended;
        });
    }

    public Identifier getCooldownGroup(ItemStack item) {
        return BuiltInRegistries.ITEM.getKey(item.getItem());
    }

    public void addCooldown(ItemStack item, int time) {
        if (owner != null) {
            owner.setCooldown(mirror(item), time);
        } else {
            addCooldown(getCooldownGroup(item), time);
        }
    }

    public void addCooldown(Identifier cooldownGroup, int time) {
        if (owner != null) {
            owner.setCooldown(net.kyori.adventure.key.Key.key(cooldownGroup.toString()), time);
            return;
        }
        local.put(cooldownGroup, new int[] {tickCount, tickCount + time});
        onCooldownStarted(cooldownGroup, time);
    }

    public void removeCooldown(Identifier cooldownGroup) {
        if (owner != null) {
            owner.setCooldown(net.kyori.adventure.key.Key.key(cooldownGroup.toString()), 0);
        } else if (local.remove(cooldownGroup) != null) {
            onCooldownEnded(cooldownGroup);
        }
    }

    protected void onCooldownStarted(Identifier cooldownGroup, int duration) {
        return;
    }

    protected void onCooldownEnded(Identifier cooldownGroup) {
        return;
    }
}
