package org.basaltmc.bukkit.facade;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.BlockAndLightGetter;

public final class SpawnLight {
    private SpawnLight() {
    }

    private static org.bukkit.block.Block block(Object level, BlockPos pos) {
        return level instanceof Level real ? real.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()) : null;
    }

    public static int sky(Object level, BlockPos pos) {
        org.bukkit.block.Block block = block(level, pos);
        return block == null ? 0 : block.getLightFromSky();
    }

    public static int raw(Object level, BlockPos pos, int skyDarkening) {
        org.bukkit.block.Block block = block(level, pos);
        return block == null ? 0 : Math.max(block.getLightFromBlocks(), block.getLightFromSky() - skyDarkening);
    }

    public static boolean thundering(Object level) {
        return level instanceof Level real && real.getWorld().isThundering();
    }

    public static boolean peaceful(Object level) {
        return level instanceof Level real && real.getWorld().getDifficulty() == org.bukkit.Difficulty.PEACEFUL;
    }
}
