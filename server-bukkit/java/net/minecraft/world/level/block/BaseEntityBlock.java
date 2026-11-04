// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.BlockHitResult;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;

public abstract class BaseEntityBlock extends Block implements EntityBlock {
    protected BaseEntityBlock(int index) {
        super(index);
    }

    @Override
    public net.minecraft.world.level.block.entity.BlockEntity newBlockEntity(net.minecraft.core.BlockPos pos,
            net.minecraft.world.level.block.state.BlockState state) {
        return org.basaltmc.bukkit.facade.BlockEntityTypes.detached(blockKey(), pos);
    }

    protected boolean hasInventory() {
        return false;
    }

    private static org.bukkit.block.Container containerAt(Level level, BlockPos pos) {
        if (level instanceof ServerLevel server
            && server.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getState() instanceof org.bukkit.block.Container container) {
            return container;
        }
        return null;
    }

    @Override
    protected InteractionResult useWithoutItem(BlockState state, Level level, BlockPos pos, Player player,
            BlockHitResult hitResult) {
        if (hasInventory() && player.getBukkitEntity() instanceof org.bukkit.entity.Player viewer) {
            org.bukkit.block.Container container = containerAt(level, pos);
            if (container != null) {
                viewer.openInventory(container.getInventory());
                return InteractionResult.SUCCESS;
            }
        }
        return super.useWithoutItem(state, level, pos, player, hitResult);
    }

    @Override
    protected boolean hasAnalogOutputSignal(BlockState state) {
        return hasInventory();
    }

    @Override
    protected int getAnalogOutputSignal(BlockState state, Level level, BlockPos pos, Direction direction) {
        org.bukkit.block.Container container = hasInventory() ? containerAt(level, pos) : null;
        if (container == null) {
            return 0;
        }
        org.bukkit.inventory.Inventory inventory = container.getInventory();
        float fill = 0.0F;
        for (org.bukkit.inventory.ItemStack stack : inventory.getContents()) {
            if (stack != null && !stack.getType().isAir()) {
                fill += (float) stack.getAmount() / Math.min(inventory.getMaxStackSize(), stack.getMaxStackSize());
            }
        }
        fill /= inventory.getSize();
        return (int) Math.floor(fill * 14.0F) + (fill > 0.0F ? 1 : 0);
    }
}
