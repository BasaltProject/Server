// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record EndPlatformFeature() implements Feature {
    public static final MapCodec<EndPlatformFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.EndPlatformFeature.class, "CODEC");
    public MapCodec<EndPlatformFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        createEndPlatform(level, origin, false);
        return true;
    }
    public static void createEndPlatform(ServerLevelAccessor newLevel, BlockPos origin, boolean dropResources) {
        BlockPos.MutableBlockPos cursor = origin.mutable();
        for (int dz = -2; dz <= 2; dz++) {
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -1; dy < 3; dy++) {
                    BlockPos at = cursor.set(origin).move(dx, dy, dz);
                    Block wanted = dy == -1 ? Blocks.OBSIDIAN : Blocks.AIR;
                    if (!newLevel.getBlockState(at).is(wanted)) {
                        if (dropResources) {
                            newLevel.destroyBlock(at, true, null);
                        }
                        newLevel.setBlockAndUpdate(at, wanted.defaultBlockState());
                    }
                }
            }
        }


    }
}
