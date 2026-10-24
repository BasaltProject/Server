package org.basaltmc.bukkit.entity.ops;

import java.util.UUID;
import net.minecraft.nbt.CompoundTag;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.entity.EntityItems;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.entity.Item;
import org.bukkit.inventory.ItemStack;

public interface ItemOps extends Item, Cored {
    int kUnlimited = -32768;

    default ItemStack getItemStack() {
        byte[] stored = core().nbt("Item");
        ItemStack stack = stored == null ? null : EntityItems.fromNbt(stored);
        return stack == null ? new ItemStack(org.bukkit.Material.AIR) : stack;
    }

    default void setItemStack(ItemStack stack) {
        CompoundTag tag = EntityItems.tag(stack);
        if (tag == null) {
            throw new IllegalArgumentException("Item stack cannot be empty");
        }
        CompoundTag patch = new CompoundTag();
        patch.put("Item", tag);
        core().mergeNbt(NbtCodec.toBytes(patch));
    }

    default int getPickupDelay() {
        return core().nbtInt("PickupDelay");
    }

    default void setPickupDelay(int delay) {
        core().nbtSet("PickupDelay", "short", Math.max(0, Math.min(Short.MAX_VALUE, delay)));
    }

    default int getHealth() {
        return core().nbtInt("Health");
    }

    default void setHealth(int health) {
        core().nbtSet("Health", "short", health);
    }

    default UUID getThrower() {
        String thrower = core().nbtText("Thrower");
        return thrower == null ? null : UUID.fromString(thrower);
    }

    default void setThrower(UUID thrower) {
        core().nbtSet("Thrower", "string", thrower == null ? "" : thrower.toString());
    }

    default boolean isUnlimitedLifetime() {
        return core().nbtInt("Age") == kUnlimited;
    }

    default void setUnlimitedLifetime(boolean unlimited) {
        core().nbtSet("Age", "short", unlimited ? kUnlimited : 0);
    }

    default UUID getOwner() {
        Object stored = EntityNbt.compound(core(), "").get("Owner");
        return stored instanceof int[] ? org.basaltmc.bukkit.item.Convert.uuidOf(stored) : null;
    }

    default void setOwner(UUID owner) {
        if (owner == null) {
            core().nbtRemove("Owner");
        } else {
            core().nbtSet("Owner", "uuid", owner.toString());
        }
    }

    default boolean willAge() {
        return !isUnlimitedLifetime();
    }

    default void setWillAge(boolean ages) {
        setUnlimitedLifetime(!ages);
    }
}
