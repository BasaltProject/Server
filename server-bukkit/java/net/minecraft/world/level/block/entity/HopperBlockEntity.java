// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.level.block.state.BlockState;

public class HopperBlockEntity extends RandomizableContainerBlockEntity implements net.minecraft.world.level.block.entity.Hopper {
    protected HopperBlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    public HopperBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("hopper"), worldPosition, blockState);
    }

    public HopperBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    @Override
    public int getContainerSize() {
        return 5;
    }

    @Override
    protected Component getDefaultName() {
        return Component.translatable("container.hopper");
    }

    @Override
    protected AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return new net.minecraft.world.inventory.HopperMenu(containerId, inventory, this);
    }

    @Override
    public double getLevelX() {
        return getBlockPos().getX() + 0.5;
    }

    @Override
    public double getLevelY() {
        return getBlockPos().getY() + 0.5;
    }

    @Override
    public double getLevelZ() {
        return getBlockPos().getZ() + 0.5;
    }

    @Override
    public boolean isGridAligned() {
        return true;
    }

    public static net.minecraft.world.item.ItemStack addItem(net.minecraft.world.Container from, net.minecraft.world.Container to,
            net.minecraft.world.item.ItemStack stack, net.minecraft.core.Direction face) {
        int[] slots = to instanceof net.minecraft.world.WorldlyContainer sided ? sided.getSlotsForFace(face) : null;
        int count = slots == null ? to.getContainerSize() : slots.length;
        net.minecraft.world.item.ItemStack rest = stack.copy();
        for (int pass = 0; pass < 2 && !rest.isEmpty(); pass++) {
            for (int i = 0; i < count && !rest.isEmpty(); i++) {
                int slot = slots == null ? i : slots[i];
                boolean allowed = to.canPlaceItem(slot, rest)
                    && (!(to instanceof net.minecraft.world.WorldlyContainer sided) || sided.canPlaceItemThroughFace(slot, rest, face));
                if (!allowed) {
                    continue;
                }
                net.minecraft.world.item.ItemStack present = to.getItem(slot);
                if (pass == 0 && !present.isEmpty() && net.minecraft.world.item.ItemStack.isSameItemSameComponents(present, rest)) {
                    int room = to.getMaxStackSize(present) - present.getCount();
                    int moved = Math.min(room, rest.getCount());
                    if (moved > 0) {
                        present.grow(moved);
                        rest.shrink(moved);
                        to.setChanged();
                    }
                } else if (pass == 1 && present.isEmpty()) {
                    int moved = Math.min(to.getMaxStackSize(rest), rest.getCount());
                    to.setItem(slot, rest.split(moved));
                    to.setChanged();
                }
            }
        }
        return rest;
    }
}
