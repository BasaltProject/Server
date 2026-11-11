// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.carver;
import net.minecraft.util.Mth;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.CarverOutput;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
public interface WorldCarver {
    Codec<WorldCarver> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.carver.WorldCarver.class, "DIRECT_CODEC");
    Codec<Holder<WorldCarver>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.carver.WorldCarver.class, "CODEC");
    Codec<HolderSet<WorldCarver>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.carver.WorldCarver.class, "LIST_CODEC");
    default int getRange() {
        return 4;
    }
    static void carveEllipsoid(ChunkPos chunkPos, double x, double y, double z, double horizontalRadius, double verticalRadius, CarverOutput output, CarveSkipChecker skipChecker) {
        double centerX = chunkPos.getMiddleBlockX();
        double centerZ = chunkPos.getMiddleBlockZ();
        double reach = 16.0 + horizontalRadius * 2.0;
        if (Math.abs(x - centerX) > reach || Math.abs(z - centerZ) > reach) {
            return;
        }
        int minXIndex = Math.max(Mth.floor(x - horizontalRadius) - chunkPos.getMinBlockX() - 1, 0);
        int maxXIndex = Math.min(Mth.floor(x + horizontalRadius) - chunkPos.getMinBlockX(), 15);
        int lowY = Math.max(Mth.floor(y - verticalRadius) - 1, output.minY());
        int highY = Math.min(Mth.floor(y + verticalRadius) + 1, output.maxY());
        int minZIndex = Math.max(Mth.floor(z - horizontalRadius) - chunkPos.getMinBlockZ() - 1, 0);
        int maxZIndex = Math.min(Mth.floor(z + horizontalRadius) - chunkPos.getMinBlockZ(), 15);
        for (int xi = minXIndex; xi <= maxXIndex; xi++) {
            double xd = (chunkPos.getBlockX(xi) + 0.5 - x) / horizontalRadius;
            for (int zi = minZIndex; zi <= maxZIndex; zi++) {
                double zd = (chunkPos.getBlockZ(zi) + 0.5 - z) / horizontalRadius;
                if (xd * xd + zd * zd >= 1.0) {
                    continue;
                }
                for (int wy = highY; wy > lowY; wy--) {
                    double yd = (wy - 0.5 - y) / verticalRadius;
                    if (!skipChecker.shouldSkip(xd, yd, zd, wy)) {
                        output.carve(xi, wy, zi);
                    }
                }
            }
        }
    }
    boolean carve(WorldGenerationContext context, RandomSource random, ChunkPos chunkPos, ChunkPos sourceChunkPos, CarverOutput output);
    boolean isStartChunk(RandomSource random);
    MapCodec<? extends WorldCarver> codec();
    static boolean canReach(ChunkPos chunkPos, double x, double z, int currentStep, int totalSteps, float thickness) {
        double dx = x - chunkPos.getMiddleBlockX();
        double dz = z - chunkPos.getMiddleBlockZ();
        double remaining = totalSteps - currentStep;
        double slack = thickness + 2.0F + 16.0F;
        return dx * dx + dz * dz - remaining * remaining <= slack * slack;
    }
    public interface CarveSkipChecker {
        boolean shouldSkip(double xd, double yd, double zd, int y);
    }
}
