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

public class ShulkerBoxBlockEntity extends RandomizableContainerBlockEntity implements net.minecraft.world.WorldlyContainer {
    protected ShulkerBoxBlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    public ShulkerBoxBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("shulker_box"), worldPosition, blockState);
    }

    public ShulkerBoxBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    @Override
    public int getContainerSize() {
        return 27;
    }

    @Override
    protected Component getDefaultName() {
        return Component.translatable("container.shulkerBox");
    }

    @Override
    protected AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return new net.minecraft.world.inventory.ShulkerBoxMenu(containerId, inventory, this);
    }

    @Override
    public int[] getSlotsForFace(net.minecraft.core.Direction direction) {
        int[] slots = new int[getContainerSize()];
        java.util.Arrays.setAll(slots, i -> i);
        return slots;
    }

    @Override
    public boolean canPlaceItemThroughFace(int slot, net.minecraft.world.item.ItemStack itemStack, net.minecraft.core.Direction direction) {
        return true;
    }

    @Override
    public boolean canTakeItemThroughFace(int slot, net.minecraft.world.item.ItemStack itemStack, net.minecraft.core.Direction direction) {
        return true;
    }

    @Override
    public void startOpen(net.minecraft.world.entity.ContainerUser user) {
        org.basaltmc.bukkit.facade.LidControl.set(this, true);
    }

    @Override
    public void stopOpen(net.minecraft.world.entity.ContainerUser user) {
        org.basaltmc.bukkit.facade.LidControl.set(this, false);
    }
}
