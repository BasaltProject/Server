// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record EndIslandFeature() implements Feature {
    public static final MapCodec<EndIslandFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.EndIslandFeature.class, "CODEC");
    public MapCodec<EndIslandFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        float radius = (float) random.nextInt(3) + 4.0F;
        for (int y = 0; radius > 0.5F; y--) {
            for (int x = Mth.floor(-radius); x <= Mth.ceil(radius); x++) {
                for (int z = Mth.floor(-radius); z <= Mth.ceil(radius); z++) {
                    if ((float) (x * x + z * z) <= (radius + 1.0F) * (radius + 1.0F)) {
                        setBlock(level, origin.offset(x, y, z), Blocks.END_STONE.defaultBlockState());
                    }
                }
            }
            radius -= (float) random.nextInt(2) + 0.5F;
        }
        return true;
    }
}
