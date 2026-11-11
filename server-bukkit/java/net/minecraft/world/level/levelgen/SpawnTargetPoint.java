// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import net.minecraft.util.Mth;
import com.mojang.serialization.Codec;
import java.util.Map;
import net.minecraft.core.Holder;
import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
public record SpawnTargetPoint(Map<Holder<DensityFunction>, Climate.Parameter> parameters) {
    public static final Codec<SpawnTargetPoint> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.SpawnTargetPoint.class, "CODEC");
    public long sampleFitness(DensitySamplerSet samplers, int blockX, int blockY, int blockZ) {
        long total = 0L;
        for (Map.Entry<Holder<DensityFunction>, Climate.Parameter> entry : parameters.entrySet()) {
            long coord = Climate.quantizeCoord(samplers.sampleValue(entry.getKey().value(), blockX, blockY, blockZ));
            total += Mth.square(entry.getValue().distance(coord));
        }
        return total;
    }
}
