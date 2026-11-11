// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record DeltaFeature(BlockState contents, BlockState rim, IntProvider size, IntProvider rimSize) implements Feature {
    private static final java.util.List<Block> kProtected = java.util.List.of(Blocks.BEDROCK, Blocks.NETHER_BRICKS, Blocks.NETHER_BRICK_FENCE, Blocks.NETHER_BRICK_STAIRS, Blocks.NETHER_WART, Blocks.CHEST, Blocks.SPAWNER);
    private boolean open(LevelAccessor level, BlockPos pos) {
        BlockState here = level.getBlockState(pos);
        if (here.is(contents.getBlock()) || kProtected.contains(here.getBlock())) {
            return false;
        }
        for (Direction side : Direction.values()) {
            boolean air = level.getBlockState(pos.relative(side)).isAir();
            if (air && side != Direction.UP || !air && side == Direction.UP) {
                return false;
            }
        }
        return true;
    }
    public static final MapCodec<DeltaFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.DeltaFeature.class, "CODEC");
    public MapCodec<DeltaFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        boolean any = false;
        boolean withRim = random.nextDouble() < 0.9;
        int rimX = withRim ? rimSize.sample(random) : 0;
        int rimZ = withRim ? rimSize.sample(random) : 0;
        boolean rimOn = withRim && rimX != 0 && rimZ != 0;
        int sx = size.sample(random);
        int sz = size.sample(random);
        int limit = Math.max(sx, sz);
        for (BlockPos spot : BlockPos.withinBoxByManhattanDistance(origin, sx, 0, sz)) {
            if (spot.distManhattan(origin) > limit) {
                break;
            }
            if (!open(level, spot)) {
                continue;
            }
            if (rimOn) {
                any = true;
                setBlock(level, spot, rim);
            }
            BlockPos shifted = spot.offset(rimX, 0, rimZ);
            if (open(level, shifted)) {
                any = true;
                setBlock(level, shifted, contents);
            }
        }
        return any;
    }
}
