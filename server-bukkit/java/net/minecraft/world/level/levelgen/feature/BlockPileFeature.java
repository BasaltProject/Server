// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record BlockPileFeature(Holder<BlockStateProvider> stateProvider) implements Feature {
    private boolean supported(LevelAccessor level, BlockPos at, RandomSource random) {
        BlockPos under = at.below();
        BlockState underState = level.getBlockState(under);
        return underState.is(Blocks.DIRT_PATH) ? random.nextBoolean() : underState.isFaceSturdy(level, under, Direction.UP);
    }
    private void put(WorldGenLevel level, BlockPos at, RandomSource random) {
        if (level.isEmptyBlock(at) && supported(level, at, random)) {
            level.setBlock(at, stateProvider.value().getState(level, random, at), 260);
        }
    }
    public static final MapCodec<BlockPileFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.BlockPileFeature.class, "CODEC");
    public MapCodec<BlockPileFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (origin.getY() < level.getMinY() + 5) {
            return false;
        }
        int radiusX = 2 + random.nextInt(2);
        int radiusZ = 2 + random.nextInt(2);
        for (BlockPos cell : BlockPos.betweenClosed(origin.offset(-radiusX, 0, -radiusZ), origin.offset(radiusX, 1, radiusZ))) {
            int dx = origin.getX() - cell.getX();
            int dz = origin.getZ() - cell.getZ();
            if ((float) (dx * dx + dz * dz) <= random.nextFloat() * 10.0F - random.nextFloat() * 6.0F) {
                put(level, cell, random);
            } else if ((double) random.nextFloat() < 0.031) {
                put(level, cell, random);
            }
        }
        return true;
    }
}
