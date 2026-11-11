// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record FillLayerFeature(int height, BlockState state) implements Feature {
    public static final MapCodec<FillLayerFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.FillLayerFeature.class, "CODEC");
    public MapCodec<FillLayerFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        int layerY = level.getMinY() + height;
        for (int dx = 0; dx < 16; dx++) {
            for (int dz = 0; dz < 16; dz++) {
                cursor.set(origin.getX() + dx, layerY, origin.getZ() + dz);
                if (level.getBlockState(cursor).isAir()) {
                    level.setBlock(cursor, state, 2);
                }
            }
        }
        return true;
    }
}
