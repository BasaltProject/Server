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

public class BarrelBlockEntity extends RandomizableContainerBlockEntity {
    protected BarrelBlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    public BarrelBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("barrel"), worldPosition, blockState);
    }

    public BarrelBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    @Override
    public int getContainerSize() {
        return 27;
    }

    @Override
    protected Component getDefaultName() {
        return Component.translatable("container.barrel");
    }

    @Override
    protected AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return net.minecraft.world.inventory.ChestMenu.threeRows(containerId, inventory, this);
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
