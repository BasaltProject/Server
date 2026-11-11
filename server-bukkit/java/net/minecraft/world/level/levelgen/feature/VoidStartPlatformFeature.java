// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.ChunkPos;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record VoidStartPlatformFeature() implements Feature {
    private static int chebyshev(int ax, int az, int bx, int bz) {
        return Math.max(Math.abs(ax - bx), Math.abs(az - bz));
    }
    public static final MapCodec<VoidStartPlatformFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.VoidStartPlatformFeature.class, "CODEC");
    public MapCodec<VoidStartPlatformFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        ChunkPos here = ChunkPos.containing(origin);
        ChunkPos anchor = ChunkPos.containing(new BlockPos(8, 3, 8));
        if (chebyshev(here.x(), here.z(), anchor.x(), anchor.z()) > 1) {
            return true;
        }
        BlockPos centre = new BlockPos(8, origin.getY() + 3, 8);
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int z = here.getMinBlockZ(); z <= here.getMaxBlockZ(); z++) {
            for (int x = here.getMinBlockX(); x <= here.getMaxBlockX(); x++) {
                if (chebyshev(centre.getX(), centre.getZ(), x, z) <= 16) {
                    cursor.set(x, centre.getY(), z);
                    level.setBlock(cursor, cursor.equals(centre) ? Blocks.COBBLESTONE.defaultBlockState() : Blocks.STONE.defaultBlockState(), 2);
                }
            }
        }
        return true;
    }
}
