// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record BlueIceFeature() implements Feature {
    public static final MapCodec<BlueIceFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.BlueIceFeature.class, "CODEC");
    public MapCodec<BlueIceFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (origin.getY() > level.getSeaLevel() - 1) {
            return false;
        }
        if (!level.getBlockState(origin).is(Blocks.WATER) && !level.getBlockState(origin.below()).is(Blocks.WATER)) {
            return false;
        }
        boolean nearPacked = false;
        for (Direction side : Direction.values()) {
            if (side != Direction.DOWN && level.getBlockState(origin.relative(side)).is(Blocks.PACKED_ICE)) {
                nearPacked = true;
                break;
            }
        }
        if (!nearPacked) {
            return false;
        }
        level.setBlock(origin, Blocks.BLUE_ICE.defaultBlockState(), 2);
        for (int attempt = 0; attempt < 200; attempt++) {
            int rise = random.nextInt(5) - random.nextInt(6);
            int spread = 3;
            if (rise < 2) {
                spread += rise / 2;
            }
            if (spread < 1) {
                continue;
            }
            BlockPos target = origin.offset(random.nextInt(spread) - random.nextInt(spread), rise, random.nextInt(spread) - random.nextInt(spread));
            BlockState found = level.getBlockState(target);
            if (found.isAir() || found.is(Blocks.WATER) || found.is(Blocks.PACKED_ICE) || found.is(Blocks.ICE)) {
                for (Direction side : Direction.values()) {
                    if (level.getBlockState(target.relative(side)).is(Blocks.BLUE_ICE)) {
                        level.setBlock(target, Blocks.BLUE_ICE.defaultBlockState(), 2);
                        break;
                    }
                }
            }
        }
        return true;
    }
}
