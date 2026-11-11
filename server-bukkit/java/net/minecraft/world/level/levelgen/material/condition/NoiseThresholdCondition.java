// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import java.util.function.DoubleSupplier;
import com.mojang.serialization.MapCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public record NoiseThresholdCondition(ResourceKey<NormalNoise> noise, double minThreshold, double maxThreshold, boolean is3d) implements MaterialCondition {
    public static final MapCodec<NoiseThresholdCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.NoiseThresholdCondition.class, "CODEC");
    public MapCodec<NoiseThresholdCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext ruleContext) {
        DoubleSupplier sampler = ruleContext.getNoiseSampler(noise, is3d);
        return () -> {
            double value = sampler.getAsDouble();
            return value >= minThreshold && value <= maxThreshold;
        };
    }
}
