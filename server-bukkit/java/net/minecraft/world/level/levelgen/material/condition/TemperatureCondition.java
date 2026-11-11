// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public enum TemperatureCondition implements MaterialCondition {
    INSTANCE;
    public static final MapCodec<TemperatureCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.TemperatureCondition.class, "CODEC");
    public MapCodec<TemperatureCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext context) {
        return () -> context.getBiome().value().coldEnoughToSnow(context.blockPos(), context.getSeaLevel());
    }
}
