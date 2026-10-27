package org.basaltmc.bukkit.entity;

import net.minecraft.nbt.CompoundTag;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.inventory.ItemStack;

public final class EntityItems {
    private EntityItems() {}

    public static CompoundTag tag(ItemStack stack) {
        if (stack == null || stack.getType().isAir() || stack.getAmount() <= 0) {
            return null;
        }
        return NbtCodec.fromBytes(org.basaltmc.bukkit.item.Nbt.writeNamed(org.basaltmc.bukkit.item.Convert.stack(stack)));
    }

    public static byte[] toNbt(ItemStack stack) {
        CompoundTag tag = tag(stack);
        return tag == null ? null : NbtCodec.toBytes(tag);
    }

    public static ItemStack fromNbt(byte[] bytes) {
        java.util.Map<String, Object> data = bytes == null ? null : org.basaltmc.bukkit.item.Nbt.readNamed(bytes);
        return data == null ? null : org.basaltmc.bukkit.item.Convert.stack(data);
    }
}
