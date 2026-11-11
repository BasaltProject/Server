// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import net.minecraft.core.QuartPos;
import net.minecraft.util.Mth;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
public class NoiseSpawnFinder {
    private Result result;
    private NoiseSpawnFinder(List<SpawnTargetPoint> targetPoints, DensitySamplerSet samplers) {
        result = evaluate(samplers, targetPoints, 0, 0);
        radialSearch(samplers, targetPoints, 2048.0F, 512.0F);
        radialSearch(samplers, targetPoints, 512.0F, 32.0F);
    }
    public static BlockPos findSpawnPosition(List<SpawnTargetPoint> targetPoints, DensitySamplerSet samplers) {
        return new NoiseSpawnFinder(targetPoints, samplers).result.location();
    }
    static record Result(BlockPos location, long fitness) {
    }
    private void radialSearch(DensitySamplerSet samplers, List<SpawnTargetPoint> targetPoints, float maxRadius, float step) {
        float angle = 0.0F;
        float radius = step;
        BlockPos origin = result.location();
        while (radius <= maxRadius) {
            Result candidate = evaluate(samplers, targetPoints, origin.getX() + (int) (Math.sin(angle) * radius),
                origin.getZ() + (int) (Math.cos(angle) * radius));
            if (candidate.fitness() < result.fitness()) {
                result = candidate;
            }
            angle += step / radius;
            if (angle > Math.PI * 2.0) {
                angle = 0.0F;
                radius += step;
            }
        }
    }
    private static Result evaluate(DensitySamplerSet samplers, List<SpawnTargetPoint> targetPoints, int blockX, int blockZ) {
        int qx = QuartPos.toBlock(QuartPos.fromBlock(blockX));
        int qz = QuartPos.toBlock(QuartPos.fromBlock(blockZ));
        long best = Long.MAX_VALUE;
        for (SpawnTargetPoint point : targetPoints) {
            best = Math.min(best, point.sampleFitness(samplers, qx, 0, qz));
        }
        long originBias = Mth.square((long) blockX) + Mth.square((long) blockZ);
        return new Result(new BlockPos(blockX, 0, blockZ), best * Mth.square(2048L) + originBias);
    }
}
