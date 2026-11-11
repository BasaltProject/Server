// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.sounds;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
public class AmbientDesertBlockSoundsPlayer {
    private static boolean sandTrigger(BlockState state) {
        return state.is(BlockTags.TRIGGERS_AMBIENT_DESERT_SAND_BLOCK_SOUNDS);
    }
    private static boolean columnTriggers(Level level, BlockPos.MutableBlockPos cursor) {
        int surfaceY = level.getHeight(Heightmap.Types.WORLD_SURFACE, cursor) - 1;
        if (Math.abs(surfaceY - cursor.getY()) > 5) {
            cursor.move(Direction.UP, 6);
            BlockState above = level.getBlockState(cursor);
            cursor.move(Direction.DOWN);
            for (int i = 0; i < 10; i++) {
                BlockState current = level.getBlockState(cursor);
                if (above.isAir() && sandTrigger(current)) {
                    return true;
                }
                above = current;
                cursor.move(Direction.DOWN);
            }
            return false;
        }
        boolean airAbove = level.getBlockState(cursor.setY(surfaceY + 1)).isAir();
        return airAbove && sandTrigger(level.getBlockState(cursor.setY(surfaceY)));
    }
    private static boolean sandNearby(Level level, BlockPos pos) {
        int found = 0;
        int checked = 0;
        BlockPos.MutableBlockPos cursor = pos.mutable();
        for (Direction dir : Direction.Plane.HORIZONTAL) {
            cursor.set(pos).move(dir, 8);
            if (columnTriggers(level, cursor) && found++ >= 3) {
                return true;
            }
            checked++;
            if (4 - checked + found < 3) {
                return false;
            }
        }
        return false;
    }
    public static void playAmbientSandSounds(Level level, BlockPos pos, RandomSource random) {
        if (level.getBlockState(pos.above()).is(Blocks.AIR) && random.nextInt(2100) == 0 && sandNearby(level, pos)) {
            level.playLocalSound((double) pos.getX(), (double) pos.getY(), (double) pos.getZ(), SoundEvents.SAND_IDLE, SoundSource.AMBIENT, 1.0F, 1.0F, false);
        }
    }
    public static void playAmbientDryGrassSounds(Level level, BlockPos pos, RandomSource random) {
        if (random.nextInt(200) == 0 && shouldPlayDesertDryVegetationBlockSounds(level, pos.below())) {
            level.playPlayerSound(SoundEvents.DRY_GRASS, SoundSource.AMBIENT, 1.0F, 1.0F);
        }
    }
    public static void playAmbientDeadBushSounds(Level level, BlockPos pos, RandomSource random) {
        if (random.nextInt(130) != 0) {
            return;
        }
        BlockState below = level.getBlockState(pos.below());
        if ((below.is(Blocks.RED_SAND) || below.is(BlockTags.TERRACOTTA)) && random.nextInt(3) != 0) {
            return;
        }
        if (shouldPlayDesertDryVegetationBlockSounds(level, pos.below())) {
            level.playLocalSound((double) pos.getX(), (double) pos.getY(), (double) pos.getZ(), SoundEvents.DEAD_BUSH_IDLE, SoundSource.AMBIENT, 1.0F, 1.0F, false);
        }
    }
    public static boolean shouldPlayDesertDryVegetationBlockSounds(Level level, BlockPos belowPos) {
        return level.getBlockState(belowPos).is(BlockTags.TRIGGERS_AMBIENT_DESERT_DRY_VEGETATION_BLOCK_SOUNDS)
            && level.getBlockState(belowPos.below()).is(BlockTags.TRIGGERS_AMBIENT_DESERT_DRY_VEGETATION_BLOCK_SOUNDS);
    }
}
