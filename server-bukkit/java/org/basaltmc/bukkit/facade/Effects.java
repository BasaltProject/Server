package org.basaltmc.bukkit.facade;

import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.WorldNatives;

public final class Effects {
    public static final int kNeighbors = 1;
    public static final int kShapes = 2;
    public static final int kLight = 4;
    public static final int kEntities = 8;
    public static final int kPoi = 16;
    public static final int kTicks = 32;
    public static final int kClients = 64;
    public static final int kPlace = 128;
    public static final int kDeparture = 256;

    private Effects() {}

    private static int[] at(BlockPos pos) {
        return new int[] {pos.getX(), pos.getY(), pos.getZ()};
    }

    private static String worldOf(LevelAccessor level) {
        return level instanceof ServerLevel server ? server.getWorld().getName() : null;
    }

    public static int ofChunkFlags(int flags) {
        int mask = kPoi;
        if ((flags & 256) == 0) {
            mask |= kEntities;
        }
        if ((flags & 512) == 0) {
            mask |= kPlace | kTicks;
        }
        if ((flags & (1 | 64)) != 0) {
            mask |= kDeparture;
        }
        return mask;
    }

    public static void placed(LevelAccessor level, BlockPos pos, BlockState oldState) {
        String world = worldOf(level);
        if (world != null) {
            WorldNatives.settleBlocks(world, at(pos), new int[] {oldState.stateId()}, kPlace | kTicks);
        }
    }

    public static void neighborsAt(LevelAccessor level, BlockPos pos, BlockState source) {
        String world = worldOf(level);
        if (world != null) {
            WorldNatives.settleBlocks(world, at(pos), new int[] {source.stateId()}, kNeighbors);
        }
    }

    public static void neighborChanged(Level level, BlockPos pos, BlockState source) {
        String world = worldOf(level);
        if (world != null) {
            WorldNatives.neighborChanged(world, at(pos), new int[] {source == null ? -1 : source.stateId()});
        }
    }

    public static void shapes(LevelAccessor level, BlockPos pos) {
        String world = worldOf(level);
        if (world != null) {
            WorldNatives.updateShapes(world, at(pos), null, false);
        }
    }

    public static void indirectShapes(LevelAccessor level, BlockPos pos, BlockState state) {
        String world = worldOf(level);
        if (world != null) {
            WorldNatives.updateShapes(world, at(pos), new int[] {state.stateId()}, true);
        }
    }

    public static boolean useItem(net.minecraft.world.item.ItemStack stack, ServerLevel level,
            net.minecraft.world.phys.BlockHitResult hit) {
        net.minecraft.core.Direction face = hit.getDirection();
        float yaw = switch (face) {
            case NORTH -> 180.0F;
            case WEST -> 90.0F;
            case EAST -> 270.0F;
            default -> 0.0F;
        };
        float pitch = face == net.minecraft.core.Direction.DOWN ? 90.0F : face == net.minecraft.core.Direction.UP ? -90.0F : 0.0F;
        BlockPos pos = hit.getBlockPos();
        return WorldNatives.useItemOn(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ(), face.ordinal(),
            stack.getItem().itemKey().toString(), yaw, pitch) > 0;
    }
}
