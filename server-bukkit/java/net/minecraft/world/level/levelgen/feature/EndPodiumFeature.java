// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record EndPodiumFeature(boolean active) implements Feature {
    private void replaceAndDrop(WorldGenLevel level, BlockPos pos, Block block) {
        if (!level.getBlockState(pos).is(block)) {
            level.destroyBlock(pos, true, null);
            setBlock(level, pos, block.defaultBlockState());
        }
    }
    public static final int PODIUM_RADIUS = 4;
    public static final int PODIUM_PILLAR_HEIGHT = 4;
    public static final int RIM_RADIUS = 1;
    public static final float CORNER_ROUNDING = 0.5F;
    public static final MapCodec<EndPodiumFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.EndPodiumFeature.class, "CODEC");
    public MapCodec<EndPodiumFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockPos low = new BlockPos(origin.getX() - 4, origin.getY() - 1, origin.getZ() - 4);
        BlockPos high = new BlockPos(origin.getX() + 4, origin.getY() + 32, origin.getZ() + 4);
        for (BlockPos spot : BlockPos.betweenClosed(low, high)) {
            boolean inner = spot.closerThan(origin, 2.5D);
            if (!inner && !spot.closerThan(origin, 3.5D)) {
                continue;
            }
            if (spot.getY() < origin.getY()) {
                if (inner) {
                    setBlock(level, spot, Blocks.BEDROCK.defaultBlockState());
                } else if (active) {
                    replaceAndDrop(level, spot, Blocks.END_STONE);
                } else {
                    setBlock(level, spot, Blocks.END_STONE.defaultBlockState());
                }
            } else if (spot.getY() > origin.getY()) {
                if (active) {
                    replaceAndDrop(level, spot, Blocks.AIR);
                } else {
                    setBlock(level, spot, Blocks.AIR.defaultBlockState());
                }
            } else if (!inner) {
                setBlock(level, spot, Blocks.BEDROCK.defaultBlockState());
            } else if (active) {
                replaceAndDrop(level, spot, Blocks.END_PORTAL);
            } else {
                setBlock(level, spot, Blocks.AIR.defaultBlockState());
            }
        }
        for (int y = 0; y < 4; y++) {
            setBlock(level, origin.above(y), Blocks.BEDROCK.defaultBlockState());
        }
        BlockPos mid = origin.above(2);
        for (Direction face : Direction.Plane.HORIZONTAL) {
            setBlock(level, mid.relative(face), org.basaltmc.bukkit.facade.StateProps.with(Blocks.WALL_TORCH.defaultBlockState(), "facing", face.getSerializedName()));
        }
        return true;
    }
}
