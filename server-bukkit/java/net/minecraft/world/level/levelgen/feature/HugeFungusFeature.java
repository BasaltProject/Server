// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.GrowingPlantHeadBlock;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
public record HugeFungusFeature(BlockState validBaseState, BlockState stemState, BlockState hatState, BlockState decorState, BlockPredicate replaceableBlocks, boolean planted) implements Feature {
    private boolean canOverwrite(WorldGenLevel level, BlockPos pos, boolean orPredicate) {
        if (level.isStateAtPosition(pos, BlockBehaviour.BlockStateBase::canBeReplaced)) {
            return true;
        }
        return orPredicate && replaceableBlocks.test(level, pos);
    }
    private void growStem(WorldGenLevel level, RandomSource random, BlockPos base, int height, boolean huge) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        int reach = huge ? 1 : 0;
        for (int dx = -reach; dx <= reach; dx++) {
            for (int dz = -reach; dz <= reach; dz++) {
                boolean corner = huge && Mth.abs(dx) == reach && Mth.abs(dz) == reach;
                for (int dy = 0; dy < height; dy++) {
                    cursor.setWithOffset(base, dx, dy, dz);
                    if (!canOverwrite(level, cursor, true)) {
                        continue;
                    }
                    if (planted) {
                        if (!level.getBlockState(cursor.below()).isAir()) {
                            level.destroyBlock(cursor, true);
                        }
                        level.setBlockAndUpdate(cursor, stemState);
                    } else if (!corner || random.nextFloat() < 0.1F) {
                        setBlock(level, cursor, stemState);
                    }
                }
            }
        }
    }
    private void growHat(WorldGenLevel level, RandomSource random, BlockPos base, int height, boolean huge) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        boolean vines = hatState.is(Blocks.NETHER_WART_BLOCK);
        int hatHeight = Math.min(random.nextInt(1 + height / 3) + 5, height);
        int startY = height - hatHeight;
        for (int dy = startY; dy <= height; dy++) {
            int radius = dy < height - random.nextInt(3) ? 2 : 1;
            if (hatHeight > 8 && dy < startY + 4) {
                radius = 3;
            }
            if (huge) {
                radius++;
            }
            for (int dx = -radius; dx <= radius; dx++) {
                for (int dz = -radius; dz <= radius; dz++) {
                    boolean edgeX = dx == -radius || dx == radius;
                    boolean edgeZ = dz == -radius || dz == radius;
                    boolean inside = !edgeX && !edgeZ && dy != height;
                    boolean corner = edgeX && edgeZ;
                    cursor.setWithOffset(base, dx, dy, dz);
                    if (!canOverwrite(level, cursor, false)) {
                        continue;
                    }
                    if (planted && !level.getBlockState(cursor.below()).isAir()) {
                        level.destroyBlock(cursor, true);
                    }
                    if (dy < startY + 3) {
                        if (!inside) {
                            hatSkirt(level, random, cursor, vines);
                        }
                    } else if (inside) {
                        hatBlock(level, random, cursor, 0.1F, 0.2F, vines ? 0.1F : 0.0F);
                    } else if (corner) {
                        hatBlock(level, random, cursor, 0.01F, 0.7F, vines ? 0.083F : 0.0F);
                    } else {
                        hatBlock(level, random, cursor, 5.0E-4F, 0.98F, vines ? 0.07F : 0.0F);
                    }
                }
            }
        }
    }
    private void hatBlock(LevelAccessor level, RandomSource random, BlockPos.MutableBlockPos pos, float decorOdds, float hatOdds, float vineOdds) {
        if (random.nextFloat() < decorOdds) {
            setBlock(level, pos, decorState);
        } else if (random.nextFloat() < hatOdds) {
            setBlock(level, pos, hatState);
            if (random.nextFloat() < vineOdds) {
                dangleVines(pos, level, random);
            }
        }
    }
    private void hatSkirt(LevelAccessor level, RandomSource random, BlockPos pos, boolean vines) {
        if (level.getBlockState(pos.below()).is(hatState.getBlock())) {
            setBlock(level, pos, hatState);
        } else if ((double) random.nextFloat() < 0.15) {
            setBlock(level, pos, hatState);
            if (vines && random.nextInt(11) == 0) {
                dangleVines(pos, level, random);
            }
        }
    }
    private static void dangleVines(BlockPos hat, LevelAccessor level, RandomSource random) {
        BlockPos.MutableBlockPos start = hat.mutable().move(Direction.DOWN);
        if (!level.isEmptyBlock(start)) {
            return;
        }
        int length = Mth.nextInt(random, 1, 5);
        if (random.nextInt(7) == 0) {
            length *= 2;
        }
        vineColumn(level, random, start, length, 23, 25);
    }
    private static void vineColumn(LevelAccessor level, RandomSource random, BlockPos start, int length, int minAge, int maxAge) {
        BlockPos.MutableBlockPos cursor = start.mutable();
        for (int step = 0; step <= length; step++) {
            if (level.isEmptyBlock(cursor)) {
                if (step == length || !level.isEmptyBlock(cursor.below())) {
                    level.setBlock(cursor, Blocks.WEEPING_VINES.defaultBlockState().setValue(GrowingPlantHeadBlock.AGE, Mth.nextInt(random, minAge, maxAge)), 2);
                    break;
                }
                level.setBlock(cursor, Blocks.WEEPING_VINES_PLANT.defaultBlockState(), 2);
            }
            cursor.move(Direction.DOWN);
        }
    }
    public static final MapCodec<HugeFungusFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.HugeFungusFeature.class, "CODEC");
    public MapCodec<HugeFungusFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!level.getBlockState(origin.below()).is(validBaseState.getBlock())) {
            return false;
        }
        int height = Mth.nextInt(random, 4, 13);
        if (random.nextInt(12) == 0) {
            height *= 2;
        }
        if (!planted && origin.getY() + height + 1 >= chunkGenerator.getGenDepth()) {
            return false;
        }
        boolean huge = !planted && random.nextFloat() < 0.06F;
        level.setBlock(origin, Blocks.AIR.defaultBlockState(), 260);
        growStem(level, random, origin, height, huge);
        growHat(level, random, origin, height, huge);
        return true;


    }
}
