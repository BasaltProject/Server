// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record DiskFeature(Holder<BlockStateProvider> stateProvider, BlockPredicate target, IntProvider radius, int halfHeight) implements Feature {
    private boolean column(WorldGenLevel level, RandomSource random, int top, int bottom, BlockPos.MutableBlockPos pos) {
        boolean any = false;
        boolean chained = false;
        for (int y = top; y > bottom; y--) {
            pos.setY(y);
            if (target.test(level, pos)) {
                BlockState chosen = stateProvider.value().getOptionalState(level, random, pos);
                if (chosen != null) {
                    level.setBlock(pos, chosen, 2);
                    if (!chained) {
                        markAboveForPostProcessing(level, pos);
                    }
                    any = true;
                    chained = true;
                }
            } else {
                chained = false;
            }
        }
        return any;
    }
    public static final MapCodec<DiskFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.DiskFeature.class, "CODEC");
    public MapCodec<DiskFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        boolean any = false;
        int top = origin.getY() + halfHeight;
        int bottom = origin.getY() - halfHeight - 1;
        int reach = radius.sample(random);
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (BlockPos spot : BlockPos.betweenClosed(origin.offset(-reach, 0, -reach), origin.offset(reach, 0, reach))) {
            int dx = spot.getX() - origin.getX();
            int dz = spot.getZ() - origin.getZ();
            if (dx * dx + dz * dz <= reach * reach) {
                any |= column(level, random, top, bottom, cursor.set(spot));
            }
        }
        return any;
    }
}
