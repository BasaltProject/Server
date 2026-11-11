// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record ReplaceBlockFeature(List<BlockReplacement> replacements) implements Feature {
    public static final MapCodec<ReplaceBlockFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.ReplaceBlockFeature.class, "CODEC");
    public MapCodec<ReplaceBlockFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        for (BlockReplacement entry : replacements) {
            if (entry.target().test(level.getBlockState(origin), origin, random)) {
                level.setBlock(origin, entry.state(), 2);
                break;
            }
        }
        return true;
    }
}
