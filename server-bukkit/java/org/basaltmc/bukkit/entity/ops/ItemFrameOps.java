package org.basaltmc.bukkit.entity.ops;

import net.minecraft.nbt.CompoundTag;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.entity.EntityItems;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.Rotation;
import org.bukkit.entity.ItemFrame;
import org.bukkit.inventory.ItemStack;

public interface ItemFrameOps extends ItemFrame, Cored {
    default ItemStack getItem() {
        byte[] stored = core().nbt("Item");
        ItemStack stack = stored == null ? null : EntityItems.fromNbt(stored);
        return stack == null ? new ItemStack(org.bukkit.Material.AIR) : stack;
    }

    default void setItem(ItemStack item) {
        setItem(item, true);
    }

    default void setItem(ItemStack item, boolean playSound) {
        CompoundTag tag = EntityItems.tag(item);
        CompoundTag patch = new CompoundTag();
        if (tag != null) {
            patch.put("Item", tag);
            core().mergeNbt(NbtCodec.toBytes(patch));
        } else {
            core().nbtSet("Item", "string", "");
        }
    }

    default Rotation getRotation() {
        return Rotation.values()[Math.floorMod(core().nbtInt("ItemRotation"), Rotation.values().length)];
    }

    default void setRotation(Rotation rotation) throws IllegalArgumentException {
        if (rotation == null) {
            throw new IllegalArgumentException("Rotation cannot be null");
        }
        core().nbtSet("ItemRotation", "byte", rotation.ordinal());
    }

    default boolean isFixed() {
        return core().nbtFlag("Fixed");
    }

    default void setFixed(boolean fixed) {
        core().nbtSet("Fixed", "bool", fixed);
    }

    default boolean isVisible() {
        return !core().flag("invisible");
    }

    default void setVisible(boolean visible) {
        core().set("invisible", !visible);
    }

    default float getItemDropChance() {
        return core().nbtReal("ItemDropChance") == 0.0 && core().nbtText("ItemDropChance") == null ? 1.0f
            : (float) core().nbtReal("ItemDropChance");
    }

    default void setItemDropChance(float chance) {
        core().nbtSet("ItemDropChance", "float", (double) chance);
    }
}
