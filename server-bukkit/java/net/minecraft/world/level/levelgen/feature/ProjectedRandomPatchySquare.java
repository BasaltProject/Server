// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record ProjectedRandomPatchySquare(Holder<BlockStateProvider> block, BlockPredicate projectThrough, IntProvider size, int maxProjectionHeight) implements Feature {
    public static final MapCodec<ProjectedRandomPatchySquare> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.ProjectedRandomPatchySquare.class, "CODEC");
    public MapCodec<ProjectedRandomPatchySquare> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos pos) {
        BlockPos.MutableBlockPos base = new BlockPos.MutableBlockPos();
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos();
        int span = size.sample(random);
        int bound = Mth.square(span) + 1;
        for (int dx = -span; dx <= span; dx++) {
            for (int dz = -span; dz <= span; dz++) {
                int weight = Mth.abs(dx) * Mth.abs(dz);
                if (random.nextInt(bound) >= bound - weight) {
                    continue;
                }
                base.setWithOffset(pos, dx, 0, dz);
                int budget = maxProjectionHeight;
                while (projectThrough.test(level, probe.setWithOffset(base, Direction.DOWN))) {
                    base.move(Direction.DOWN);
                    if (--budget <= 0) {
                        break;
                    }
                }
                BlockState chosen = block.value().getOptionalState(level, random, base);
                if (chosen != null) {
                    level.setBlock(base, chosen, 2);
                }
            }
        }
        return true;
    }
}
