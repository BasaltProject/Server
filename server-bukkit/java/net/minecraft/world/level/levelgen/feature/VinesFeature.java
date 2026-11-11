// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record VinesFeature() implements Feature {
    public static final MapCodec<VinesFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.VinesFeature.class, "CODEC");
    public MapCodec<VinesFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!level.isEmptyBlock(origin)) {
            return false;
        }
        for (Direction side : Direction.values()) {
            if (side != Direction.DOWN && net.minecraft.world.level.block.VineBlock.isAcceptableNeighbour(level, origin.relative(side), side)) {
                level.setBlock(origin, Blocks.VINE.defaultBlockState().setValue(net.minecraft.world.level.block.VineBlock.getPropertyForFace(side), true), 2);
                return true;
            }
        }
        return false;
    }
}
