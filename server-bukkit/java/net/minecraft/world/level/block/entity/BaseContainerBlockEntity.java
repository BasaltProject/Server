// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import com.mojang.serialization.DataResult;
import net.minecraft.core.BlockPos;
import net.minecraft.core.NonNullList;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.Container;
import net.minecraft.world.MenuProvider;
import net.minecraft.world.Nameable;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;

public abstract class BaseContainerBlockEntity extends BlockEntity implements Container, MenuProvider, Nameable {
    private static final String kItemsKey = "Items";
    private static final String kSlotKey = "Slot";
    private static final String kNameKey = "CustomName";

    protected BaseContainerBlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    protected BaseContainerBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public boolean isLocked() {
        return nbt().contains("lock");
    }

    protected final NonNullList<ItemStack> readItems(int size) {
        NonNullList<ItemStack> items = NonNullList.withSize(size, ItemStack.EMPTY);
        ListTag list = nbt().getListOrEmpty(kItemsKey);
        for (int i = 0; i < list.size(); i++) {
            if (!(list.get(i) instanceof CompoundTag entry)) {
                continue;
            }
            int slot = entry.getByteOr(kSlotKey, (byte) -1) & 0xFF;
            CompoundTag stackTag = entry.copy();
            stackTag.remove(kSlotKey);
            ItemStack.CODEC.parse(NbtOps.INSTANCE, stackTag).result().ifPresent(stack -> {
                if (slot < size) {
                    items.set(slot, stack);
                }
            });
        }
        return items;
    }

    protected final void writeItems(NonNullList<ItemStack> items) {
        ListTag list = new ListTag();
        for (int slot = 0; slot < items.size(); slot++) {
            ItemStack stack = items.get(slot);
            if (stack.isEmpty()) {
                continue;
            }
            DataResult<Tag> encoded = ItemStack.CODEC.encodeStart(NbtOps.INSTANCE, stack);
            if (encoded.result().orElse(null) instanceof CompoundTag entry) {
                entry.putByte(kSlotKey, (byte) slot);
                list.add(entry);
            }
        }
        CompoundTag tag = nbt();
        tag.put(kItemsKey, list);
        load(tag);
    }

    @Override
    public Component getName() {
        Component custom = getCustomName();
        return custom != null ? custom : getDefaultName();
    }

    @Override
    public Component getDisplayName() {
        return getName();
    }

    @Override
    public Component getCustomName() {
        Tag name = nbt().get(kNameKey);
        return name == null ? null : ComponentSerialization.CODEC.parse(NbtOps.INSTANCE, name).result().orElse(null);
    }

    public void setCustomName(Component name) {
        CompoundTag tag = nbt();
        if (name == null) {
            tag.remove(kNameKey);
        } else {
            ComponentSerialization.CODEC.encodeStart(NbtOps.INSTANCE, name).result().ifPresent(encoded -> tag.put(kNameKey, encoded));
        }
        load(tag);
    }

    protected abstract Component getDefaultName();

    protected abstract NonNullList<ItemStack> getItems();

    protected abstract void setItems(NonNullList<ItemStack> items);

    @Override
    public boolean isEmpty() {
        for (ItemStack stack : getItems()) {
            if (!stack.isEmpty()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public ItemStack getItem(int slot) {
        return getItems().get(slot);
    }

    @Override
    public ItemStack removeItem(int slot, int count) {
        NonNullList<ItemStack> items = getItems();
        ItemStack removed = count <= 0 || items.get(slot).isEmpty() ? ItemStack.EMPTY : items.get(slot).split(count);
        if (!removed.isEmpty()) {
            setItems(items);
        }
        return removed;
    }

    @Override
    public ItemStack removeItemNoUpdate(int slot) {
        NonNullList<ItemStack> items = getItems();
        ItemStack removed = items.get(slot);
        items.set(slot, ItemStack.EMPTY);
        setItems(items);
        return removed;
    }

    @Override
    public void setItem(int slot, ItemStack itemStack) {
        NonNullList<ItemStack> items = getItems();
        items.set(slot, itemStack);
        itemStack.limitSize(getMaxStackSize(itemStack));
        setItems(items);
    }

    @Override
    public boolean stillValid(Player player) {
        return Container.stillValidBlockEntity(this, player);
    }

    @Override
    public void clearContent() {
        setItems(NonNullList.withSize(getContainerSize(), ItemStack.EMPTY));
    }

    @Override
    public AbstractContainerMenu createMenu(int containerId, Inventory inventory, Player player) {
        return createMenu(containerId, inventory);
    }

    protected abstract AbstractContainerMenu createMenu(int containerId, Inventory inventory);
}
