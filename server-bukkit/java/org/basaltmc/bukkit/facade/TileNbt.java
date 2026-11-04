package org.basaltmc.bukkit.facade;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.world.level.block.entity.BlockEntity;

/** Read-modify-write helpers for block entity views over the native NBT. */
public final class TileNbt {
    private TileNbt() {}

    public static Component component(BlockEntity tile, String key) {
        Tag stored = tile.nbt().get(key);
        return stored == null ? null : ComponentSerialization.CODEC.parse(NbtOps.INSTANCE, stored).result().orElse(null);
    }

    public static void setComponent(BlockEntity tile, String key, Component value) {
        CompoundTag tag = tile.nbt();
        if (value == null) {
            tag.remove(key);
        } else {
            ComponentSerialization.CODEC.encodeStart(NbtOps.INSTANCE, value).result().ifPresent(encoded -> tag.put(key, encoded));
        }
        tile.load(tag);
    }

    public static void update(BlockEntity tile, java.util.function.Consumer<CompoundTag> change) {
        CompoundTag tag = tile.nbt();
        change.accept(tag);
        tile.load(tag);
    }
}
