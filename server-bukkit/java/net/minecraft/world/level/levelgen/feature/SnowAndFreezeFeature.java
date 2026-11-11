// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record SnowAndFreezeFeature() implements Feature {
    public static final MapCodec<SnowAndFreezeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SnowAndFreezeFeature.class, "CODEC");
    public MapCodec<SnowAndFreezeFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockPos.MutableBlockPos top = new BlockPos.MutableBlockPos();
        BlockPos.MutableBlockPos under = new BlockPos.MutableBlockPos();
        for (int dx = 0; dx < 16; dx++) {
            for (int dz = 0; dz < 16; dz++) {
                int x = origin.getX() + dx;
                int z = origin.getZ() + dz;
                top.set(x, level.getHeight(Heightmap.Types.MOTION_BLOCKING, x, z), z);
                under.set(top).move(Direction.DOWN, 1);
                net.minecraft.world.level.biome.Biome biome = level.getBiome(top).value();
                if (biome.shouldFreeze(level, under, false)) {
                    level.setBlock(under, Blocks.ICE.defaultBlockState(), 2);
                }
                if (biome.shouldSnow(level, top)) {
                    level.setBlock(top, Blocks.SNOW.defaultBlockState(), 2);
                    BlockState belowState = level.getBlockState(under);
                    if (belowState.hasProperty(net.minecraft.world.level.block.SnowyBlock.SNOWY)) {
                        level.setBlock(under, belowState.setValue(net.minecraft.world.level.block.SnowyBlock.SNOWY, true), 2);
                    }
                }
            }
        }
        return true;
    }
}
