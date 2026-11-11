// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
public record SpikeFeature(BlockState state, BlockPredicate canPlaceOn, BlockPredicate canReplace) implements Feature {
    public static final MapCodec<SpikeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SpikeFeature.class, "CODEC");
    public MapCodec<SpikeFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockPos top = origin;
        while (level.isEmptyBlock(top) && top.getY() > level.getMinY() + 2) {
            top = top.below();
        }
        if (!canPlaceOn.test(level, top)) {
            return false;
        }
        top = top.above(random.nextInt(4));
        int height = random.nextInt(4) + 7;
        int width = height / 4 + random.nextInt(2);
        if (width > 1 && random.nextInt(60) == 0) {
            top = top.above(10 + random.nextInt(30));
        }
        for (int layer = 0; layer < height; layer++) {
            float scale = (1.0F - (float) layer / (float) height) * (float) width;
            int reach = Mth.ceil(scale);
            for (int ox = -reach; ox <= reach; ox++) {
                float fx = (float) Mth.abs(ox) - 0.25F;
                for (int oz = -reach; oz <= reach; oz++) {
                    float fz = (float) Mth.abs(oz) - 0.25F;
                    boolean insideCircle = ox == 0 && oz == 0 || !(fx * fx + fz * fz > scale * scale);
                    boolean onRim = ox == -reach || ox == reach || oz == -reach || oz == reach;
                    if (!insideCircle || (onRim && random.nextFloat() > 0.75F)) {
                        continue;
                    }
                    BlockPos upper = top.offset(ox, layer, oz);
                    if (level.getBlockState(upper).isAir() || canReplace.test(level, upper)) {
                        setBlock(level, upper, state);
                    }
                    if (layer != 0 && reach > 1) {
                        BlockPos lower = top.offset(ox, -layer, oz);
                        if (level.getBlockState(lower).isAir() || canReplace.test(level, lower)) {
                            setBlock(level, lower, state);
                        }
                    }
                }
            }
        }
        int pillar = Math.max(0, Math.min(width - 1, 1));
        for (int ox = -pillar; ox <= pillar; ox++) {
            for (int oz = -pillar; oz <= pillar; oz++) {
                BlockPos cursor = top.offset(ox, -1, oz);
                int run = 50;
                if (Math.abs(ox) == 1 && Math.abs(oz) == 1) {
                    run = random.nextInt(5);
                }
                while (cursor.getY() > 50) {
                    BlockState found = level.getBlockState(cursor);
                    if (!found.isAir() && !canReplace.test(level, cursor) && found != state) {
                        break;
                    }
                    setBlock(level, cursor, state);
                    cursor = cursor.below();
                    if (--run <= 0) {
                        cursor = cursor.below(random.nextInt(5) + 1);
                        run = random.nextInt(5);
                    }
                }
            }
        }
        return true;


    }
}
