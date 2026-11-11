// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record ChorusPlantFeature() implements Feature {
    public static final MapCodec<ChorusPlantFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.ChorusPlantFeature.class, "CODEC");
    public MapCodec<ChorusPlantFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (level.isEmptyBlock(origin) && level.getBlockState(origin.below()).is(net.minecraft.tags.BlockTags.SUPPORTS_CHORUS_PLANT)) {
            net.minecraft.world.level.block.ChorusFlowerBlock.generatePlant(level, origin, random, 8);
            return true;
        }
        return false;
    }
}
