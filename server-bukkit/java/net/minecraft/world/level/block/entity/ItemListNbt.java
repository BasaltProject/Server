// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import com.mojang.serialization.DataResult;
import net.minecraft.core.NonNullList;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.world.item.ItemStack;

final class ItemListNbt {
    private ItemListNbt() {}

    static NonNullList<ItemStack> read(CompoundTag tag, String key, int size) {
        NonNullList<ItemStack> items = NonNullList.withSize(size, ItemStack.EMPTY);
        ListTag list = tag.getListOrEmpty(key);
        for (int i = 0; i < list.size(); i++) {
            if (!(list.get(i) instanceof CompoundTag entry)) {
                continue;
            }
            int slot = entry.getByteOr("Slot", (byte) -1) & 0xFF;
            CompoundTag stackTag = entry.copy();
            stackTag.remove("Slot");
            if (slot < size) {
                ItemStack.CODEC.parse(NbtOps.INSTANCE, stackTag).result().ifPresent(stack -> items.set(slot, stack));
            }
        }
        return items;
    }

    static void write(CompoundTag tag, String key, NonNullList<ItemStack> items) {
        ListTag list = new ListTag();
        for (int slot = 0; slot < items.size(); slot++) {
            if (items.get(slot).isEmpty()) {
                continue;
            }
            DataResult<Tag> encoded = ItemStack.CODEC.encodeStart(NbtOps.INSTANCE, items.get(slot));
            if (encoded.result().orElse(null) instanceof CompoundTag entry) {
                entry.putByte("Slot", (byte) slot);
                list.add(entry);
            }
        }
        tag.put(key, list);
    }

    static NonNullList<ItemStack> live(BlockEntity entity, String key, int size) {
        return read(entity.nbt(), key, size);
    }

    static void edit(BlockEntity entity, String key, int size, java.util.function.Consumer<NonNullList<ItemStack>> change) {
        CompoundTag tag = entity.nbt();
        NonNullList<ItemStack> items = read(tag, key, size);
        change.accept(items);
        write(tag, key, items);
        entity.load(tag);
    }
}
