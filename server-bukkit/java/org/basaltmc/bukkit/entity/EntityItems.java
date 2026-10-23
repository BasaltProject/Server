package org.basaltmc.bukkit.entity;

import net.minecraft.nbt.CompoundTag;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

/** The one place entity state turns an item stack into the stack NBT the server stores, and back. */
public final class EntityItems {
    private EntityItems() {}

    public static CompoundTag tag(ItemStack stack) {
        if (stack == null || stack.getType().isAir() || stack.getAmount() <= 0) {
            return null;
        }
        CompoundTag tag = new CompoundTag();
        tag.putString("id", stack.getType().getKey().toString());
        tag.putInt("count", stack.getAmount());
        return tag;
    }

    public static byte[] toNbt(ItemStack stack) {
        CompoundTag tag = tag(stack);
        return tag == null ? null : NbtCodec.toBytes(tag);
    }

    public static ItemStack fromNbt(byte[] bytes) {
        CompoundTag tag = bytes == null ? null : NbtCodec.fromBytes(bytes);
        if (tag == null) {
            return null;
        }
        Material material = Material.matchMaterial(tag.getStringOr("id", ""));
        if (material == null || material.isAir()) {
            return null;
        }
        return new ItemStack(material, Math.max(1, tag.getIntOr("count", 1)));
    }
}
