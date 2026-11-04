// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class BlastFurnaceBlockEntity extends AbstractFurnaceBlockEntity {
    public BlastFurnaceBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("blast_furnace"), worldPosition, blockState);
    }

    public BlastFurnaceBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    @Override
    protected Component getDefaultName() {
        return Component.translatable("container.blast_furnace");
    }

    @Override
    protected AbstractContainerMenu createFurnaceMenu(int containerId, Inventory inventory) {
        return new net.minecraft.world.inventory.BlastFurnaceMenu(containerId, inventory, this, dataAccess);
    }
}
