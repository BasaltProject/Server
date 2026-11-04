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

public class ChestBlockEntity extends RandomizableContainerBlockEntity implements LidBlockEntity {
    protected ChestBlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    public ChestBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("chest"), worldPosition, blockState);
    }

    public ChestBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    @Override
    public int getContainerSize() {
        return 27;
    }

    @Override
    protected Component getDefaultName() {
        return Component.translatable("container.chest");
    }

    @Override
    protected AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return net.minecraft.world.inventory.ChestMenu.threeRows(containerId, inventory, this);
    }

    @Override
    public float getOpenNess(float a) {
        return org.basaltmc.bukkit.facade.LidControl.openness(this);
    }
    public static int getOpenCount(net.minecraft.world.level.BlockGetter level, BlockPos pos) {
        if (level instanceof net.minecraft.world.level.Level real
                && real.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getState() instanceof org.bukkit.block.Chest chest) {
            return chest.getInventory().getViewers().size();
        }
        return 0;
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
